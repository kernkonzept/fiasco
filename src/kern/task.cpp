INTERFACE:

#include "context.h"
#include "kobject.h"
#include "l4_types.h"
#include "rcupdate.h"
#include "space.h"
#include "spin_lock.h"
#include "unique_ptr.h"

/**
 * \brief A task is a protection domain.
 *
 * A is derived from Space, which aggregates a set of address spaces.
 * Additionally to a space, a task provides initialization and
 * destruction functionality for a protection domain.
 * Task is also derived from Rcu_item to provide RCU shutdown of tasks.
 */
class Task :
  public cxx::Dyn_castable<Task, Kobject>,
  public Space
{
  friend class Jdb_space;

private:
  /// \brief Do host (platform) specific initialization.
  void ux_init();

public:
  enum Operation
  {
    Map         = 0,
    Unmap       = 1,
    Cap_info    = 2,
    Add_ku_mem  = 3,
    Ldt_set_x86 = 0x11,
  };

  virtual int resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode);

private:
  /// map the global utcb pointer page into this task
  void map_utcb_ptr_page();
};


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "atomic.h"
#include "config.h"
#include "entry_frame.h"
#include "globals.h"
#include "kdb_ke.h"
#include "kmem.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "l4_types.h"
#include "logdefs.h"
#include "map_util.h"
#include "mem_layout.h"
#include "ram_quota.h"
#include "thread_state.h"
#include "paging.h"

JDB_DEFINE_TYPENAME(Task, "\033[31mTask\033[m");
static Kmem_slab_t<Task::Ku_mem> _k_u_mem_list_alloc("Ku_mem");
Slab_cache *Space::Ku_mem::a = _k_u_mem_list_alloc.slab();

extern "C" void vcpu_resume(Trap_state *, Return_frame *sp)
   FIASCO_FASTCALL FIASCO_NORETURN;

IMPLEMENT_DEFAULT
int
Task::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  Trap_state ts;
  ctxt->copy_and_sanitize_trap_state(&ts, &vcpu->_regs.s);

  // FIXME: UX is currently broken
  /* UX:ctxt->vcpu_resume_user_arch(); */
  if (user_mode)
    {
      ctxt->state_add_dirty(Thread_vcpu_user);
      vcpu->state |= Vcpu_state::F_traps | Vcpu_state::F_exceptions
                     | Vcpu_state::F_debug_exc;

      ctxt->vcpu_pv_switch_to_user(vcpu, true);
    }

  ctxt->space_ref()->user_mode(user_mode);
  switchin_context(ctxt->space());
  vcpu_resume(&ts, ctxt->regs());
}

PUBLIC virtual
bool
Task::put()
{ return dec_ref() == 0; }

PRIVATE
int
Task::alloc_ku_mem_chunk(User<void>::Ptr u_addr, unsigned size, void **k_addr)
{
  assert ((size & (size - 1)) == 0);

  Kmem_alloc *const alloc = Kmem_alloc::allocator();
  void *p = alloc->q_unaligned_alloc(ram_quota(), size);

  if (EXPECT_FALSE(!p))
    return -L4_err::ENomem;

  // clean up utcbs
  memset(p, 0, size);

  Virt_addr base((Address)p);
  Mem_space::Page_order page_size(Config::PAGE_SHIFT);

  // the following works because the size is a power of two
  // and once we have size larger than a super page we have
  // always multiples of superpages
  if (size >= Config::SUPERPAGE_SIZE)
    page_size = Mem_space::Page_order(Config::SUPERPAGE_SHIFT);

  for (Virt_size i = Virt_size(0); i < Virt_size(size);
       i += Virt_size(1) << page_size)
    {
      Virt_addr kern_va = base + i;
      Virt_addr user_va = Virt_addr((Address)u_addr.get()) + i;
      Mem_space::Phys_addr pa(pmem_to_phys(Virt_addr::val(kern_va)));

      // must be valid physical address
      assert(pa != Mem_space::Phys_addr(~0UL));

      Mem_space::Status res =
        static_cast<Mem_space*>(this)->v_insert(pa, user_va, page_size,
            Mem_space::Attr(L4_fpage::Rights::URW()));

      switch (res)
        {
        case Mem_space::Insert_ok: break;
        case Mem_space::Insert_err_nomem:
          free_ku_mem_chunk(p, u_addr, size, Virt_size::val(i));
          return -L4_err::ENomem;

        case Mem_space::Insert_err_exists:
          free_ku_mem_chunk(p, u_addr, size, Virt_size::val(i));
          return -L4_err::EExists;

        default:
          printf("UTCB mapping failed: va=%p, ph=%p, res=%d\n",
              (void*)Virt_addr::val(user_va), (void*)Virt_addr::val(kern_va), res);
          kdb_ke("BUG in utcb allocation");
          free_ku_mem_chunk(p, u_addr, size, Virt_size::val(i));
          return 0;
        }
    }

  *k_addr = p;
  return 0;
}


PUBLIC
int
Task::alloc_ku_mem(L4_fpage ku_area)
{
  if (ku_area.order() < Config::PAGE_SHIFT || ku_area.order() > 20)
    return -L4_err::EInval;

  Mword sz = 1UL << ku_area.order();

  Ku_mem *m = new (ram_quota()) Ku_mem();

  if (!m)
    return -L4_err::ENomem;

  User<void>::Ptr u_addr((void*)Virt_addr::val(ku_area.mem_address()));

  void *p = 0;
  if (int e = alloc_ku_mem_chunk(u_addr, sz, &p))
    {
      m->free(ram_quota());
      return e;
    }

  m->u_addr = u_addr;
  m->k_addr = p;
  m->size = sz;

  _ku_mem.add(m, mp_cas<cxx::S_list_item*>);

  return 0;
}

PRIVATE inline NOEXPORT
void
Task::free_ku_mem(Ku_mem *m)
{
  free_ku_mem_chunk(m->k_addr, m->u_addr, m->size, m->size);
  m->free(ram_quota());
}

PRIVATE
void
Task::free_ku_mem_chunk(void *k_addr, User<void>::Ptr u_addr, unsigned size,
                        unsigned mapped_size)
{

  Kmem_alloc * const alloc = Kmem_alloc::allocator();
  Mem_space::Page_order page_size(Config::PAGE_SHIFT);

  // the following works because the size is a power of two
  // and once we have size larger than a super page we have
  // always multiples of superpages
  if (size >= Config::SUPERPAGE_SIZE)
    page_size = Mem_space::Page_order(Config::SUPERPAGE_SHIFT);

  for (Virt_size i = Virt_size(0); i < Virt_size(mapped_size);
       i += Virt_size(1) << page_size)
    {
      Virt_addr user_va = Virt_addr((Address)u_addr.get()) + i;
      static_cast<Mem_space*>(this)->v_delete(user_va, page_size, L4_fpage::Rights::FULL());
    }

  alloc->q_unaligned_free(ram_quota(), size, k_addr);
}

PRIVATE
void
Task::free_ku_mem()
{
  while (Ku_mem *m = _ku_mem.pop_front())
    free_ku_mem(m);
}


/** Allocate space for the UTCBs of all threads in this task.
 *  @ return true on success, false if not enough memory for the UTCBs
 */
PUBLIC
bool
Task::initialize()
{
  if (!Mem_space::initialize())
    return false;

  // For UX, map the UTCB pointer page. For ia32, do nothing
  map_utcb_ptr_page();

  return true;
}

/**
 * \brief Create a normal Task.
 * \pre \a parent must be valid and exist.
 */
PUBLIC explicit
Task::Task(Ram_quota *q, Caps c) : Space(q, c)
{
  ux_init();

  // increment reference counter from zero
  inc_ref(true);
}

PUBLIC explicit
Task::Task(Ram_quota *q)
: Space(q, Caps::mem() | Caps::io() | Caps::obj() | Caps::threads())
{
  ux_init();

  // increment reference counter from zero
  inc_ref(true);
}

PROTECTED
Task::Task(Ram_quota *q, Mem_space::Dir_type* pdir, Caps c)
: Space(q, pdir, c)
{
  // increment reference counter from zero
  inc_ref(true);
}

// The allocator for tasks
static Kmem_slab_t<Task> _task_allocator("Task");

PROTECTED static
Slab_cache*
Task::allocator()
{ return _task_allocator.slab(); }

PUBLIC //inline
void
Task::operator delete (void *ptr)
{
  Task *t = reinterpret_cast<Task*>(ptr);
  LOG_TRACE("Kobject delete", "del", current(), Log_destroy,
            l->id = t->dbg_id();
            l->obj = t;
            l->type = cxx::Typeid<Task>::get();
            l->ram = t->ram_quota()->current());

  Kmem_slab_t<Task>::q_free(t->ram_quota(), ptr);
}

PUBLIC template<typename TASK_TYPE, bool MUST_SYNC_KERNEL = true,
                int UTCB_AREA_MR = 0> static
TASK_TYPE * FIASCO_FLATTEN
Task::create(Ram_quota *q,
             L4_msg_tag t, Utcb const *u,
             int *err)
{
  static_assert(UTCB_AREA_MR == 0 || UTCB_AREA_MR >= 2,
                "invalid value for UTCB_AREA_MR");
  if (UTCB_AREA_MR >= 2 && EXPECT_FALSE(t.words() <= UTCB_AREA_MR))
    {
      *err = L4_err::EInval;
      return 0;
    }

  typedef Kmem_slab_t<TASK_TYPE> Alloc;
  *err = L4_err::ENomem;
  cxx::unique_ptr<TASK_TYPE> v(Alloc::q_new(q, q));

  if (EXPECT_FALSE(!v))
    return 0;

  if (EXPECT_FALSE(!v->initialize()))
    return 0;

  if (MUST_SYNC_KERNEL && (v->sync_kernel() < 0))
    return 0;

  if (UTCB_AREA_MR >= 2)
    {
      L4_fpage utcb_area(access_once(&u->values[UTCB_AREA_MR]));
      if (utcb_area.is_valid())
        {
          int e = v->alloc_ku_mem(utcb_area);
          if (e < 0)
            {
              *err = -e;
              return 0;
            }
        }
    }

  return v.release();
}

PUBLIC template<typename TASK_TYPE, bool MUST_SYNC_KERNEL = false,
                int UTCB_AREA_MR = 0> static
Kobject_iface * FIASCO_FLATTEN
Task::generic_factory(Ram_quota *q, Space *,
                      L4_msg_tag t, Utcb const *u,
                      int *err)
{
  return create<TASK_TYPE, MUST_SYNC_KERNEL, UTCB_AREA_MR>(q, t, u, err);
}

/**
 * \brief Shutdown the task.
 *
 * Currently:
 * -# Unbind and delete all contexts bound to this task.
 * -# Unmap everything from all spaces.
 * -# Delete child tasks.
 */
PUBLIC
void
Task::destroy(Kobject ***reap_list)
{
  Kobject::destroy(reap_list);

  fpage_unmap(this, L4_fpage::all_spaces(L4_fpage::Rights::FULL()), L4_map_mask::full(), reap_list);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_map(L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb)
{
  LOG_TRACE("Task map", "map", ::current(), Log_unmap,
      l->id = dbg_id();
      l->mask  = utcb->values[1];
      l->fpage = utcb->values[2]);

  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CW())))
    return commit_result(-L4_err::EPerm);

  L4_msg_tag tag = f->tag();

  L4_fpage::Rights mask;
  Task *_from = Ko::deref<Task>(&tag, utcb, &mask);
  if (!_from)
    return tag;

  L4_fpage sfp(utcb->values[2]);

  if (sfp.type() == L4_fpage::Obj)
    {
      // handle Rights::CS() bit masking for capabilities
      mask &= rights;
      mask |= L4_fpage::Rights::CD() | L4_fpage::Rights::CRW();

      // diminish when sending via restricted ipc gates
      sfp.mask_rights(mask);
    }

  Kobject::Reap_list rl;
  L4_error ret;

    {
      Ref_ptr<Task> from(_from);
      Ref_ptr<Task> self(this);
      // enforce lock order to prevent deadlocks.
      // always take lock from task with the lower memory address first
      Lock_guard_2<Lock> guard;

      // FIXME: avoid locking the current task, it is not needed
      if (!guard.check_and_lock(&existence_lock, &from->existence_lock))
        return commit_result(-L4_err::EInval);

      cpu_lock.clear();

      ret = fpage_map(from.get(), sfp, this,
                      L4_fpage::all_spaces(), L4_msg_item(utcb->values[1]), &rl);
      cpu_lock.lock();
    }

  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();

  // FIXME: treat reaped stuff
  if (ret.ok())
    return commit_result(0);
  else
    return commit_error(utcb, ret);
}


PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_unmap(Syscall_frame *f, Utcb *utcb)
{
  Kobject::Reap_list rl;
  unsigned words = f->tag().words();

  LOG_TRACE("Task unmap", "unm", ::current(), Log_unmap,
            l->id = dbg_id();
            l->mask  = utcb->values[1];
            l->fpage = utcb->values[2]);

    {
      Ref_ptr<Task> self(this);
      Lock_guard<Lock> guard;

      // FIXME: avoid locking the current task, it is not needed
      if (!guard.check_and_lock(&existence_lock))
        return commit_error(utcb, L4_error::Not_existent);

      cpu_lock.clear();

      L4_map_mask m(utcb->values[1]);

      for (unsigned i = 2; i < words; ++i)
        {
          L4_fpage::Rights const flushed
            = fpage_unmap(this, L4_fpage(utcb->values[i]), m, rl.list());

          utcb->values[i] = (utcb->values[i] & ~0xfUL)
                          | cxx::int_value<L4_fpage::Rights>(flushed);
        }
      cpu_lock.lock();
    }

  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();

  return commit_result(0, words);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_cap_valid(Syscall_frame *, Utcb *utcb)
{
  L4_obj_ref obj(utcb->values[1]);

  if (obj.special())
    return commit_result(0);

  Obj_space::Capability cap = lookup(obj.cap());
  if (EXPECT_TRUE(cap.valid()))
    {
      if (!(utcb->values[1] & 1))
        return commit_result(1);
      else
        return commit_result(cap.obj()->map_root()->cap_ref_cnt());
    }
  else
    return commit_result(0);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_caps_equal(Syscall_frame *, Utcb *utcb)
{
  L4_obj_ref obj_a(utcb->values[1]);
  L4_obj_ref obj_b(utcb->values[2]);

  if (obj_a == obj_b)
    return commit_result(1);

  if (obj_a.special() || obj_b.special())
    return commit_result(obj_a.special_cap() == obj_b.special_cap());

  Obj_space::Capability c_a = lookup(obj_a.cap());
  Obj_space::Capability c_b = lookup(obj_b.cap());

  return commit_result(c_a == c_b);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_add_ku_mem(Syscall_frame *f, Utcb *utcb)
{
  unsigned const w = f->tag().words();
  for (unsigned i = 1; i < w; ++i)
    {
      L4_fpage ku_fp(utcb->values[i]);
      if (!ku_fp.is_valid() || !ku_fp.is_mempage())
        return commit_result(-L4_err::EInval);

      int e = alloc_ku_mem(ku_fp);
      if (e < 0)
        return commit_result(e);
    }

  return commit_result(0);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_cap_info(Syscall_frame *f, Utcb *utcb)
{
  L4_msg_tag const &tag = f->tag();

  switch (tag.words())
    {
    default: return commit_result(-L4_err::EInval);
    case 2:  return sys_cap_valid(f, utcb);
    case 3:  return sys_caps_equal(f, utcb);
    }
}


PUBLIC
void
Task::invoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb)
{
  if (EXPECT_FALSE(f->tag().proto() != L4_msg_tag::Label_task))
    {
      f->tag(commit_result(-L4_err::EBadproto));
      return;
    }

  switch (utcb->values[0])
    {
    case Map:
      f->tag(sys_map(rights, f, utcb));
      return;
    case Unmap:
      f->tag(sys_unmap(f, utcb));
      return;
    case Cap_info:
      f->tag(sys_cap_info(f, utcb));
      return;
    case Add_ku_mem:
      f->tag(sys_add_ku_mem(f, utcb));
      return;
    default:
      L4_msg_tag tag = f->tag();
      if (invoke_arch(tag, utcb))
        f->tag(tag);
      else
        f->tag(commit_result(-L4_err::ENosys));
      return;
    }
}

namespace {
static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_task,
                             &Task::generic_factory<Task, true, 2>);
}
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!ux]:

IMPLEMENT inline void Task::map_utcb_ptr_page() {}
IMPLEMENT inline void Task::ux_init() {}

PUBLIC inline
Task::~Task()
{ free_ku_mem(); }


// ---------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Task
{
private:
  struct Log_unmap : public Tb_entry
  {
    Mword id;
    Mword mask;
    Mword fpage;
    void print(String_buffer *buf) const;
  };

};

// ---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

IMPLEMENT
void
Task::Log_unmap::print(String_buffer *buf) const
{
  L4_fpage fp(fpage);
  buf->printf("task=[U:%lx] mask=%lx fpage=[%u/%u]%lx",
              id, mask, (unsigned)fp.order(), (unsigned)fp.type(), fpage);
}
