INTERFACE:

#include "context.h"
#include "kobject.h"
#include "l4_types.h"
#include "space.h"
#include "spin_lock.h"
#include "unique_ptr.h"

/**
 * A task is a protection domain.
 *
 * A task is derived from Space, which aggregates a set of address spaces.
 * Additionally to a space, a task provides initialization and destruction
 * functionality for a protection domain.
 */
class Task :
  public cxx::Dyn_castable<Task, Kobject>,
  public Space
{
  friend class Jdb_space;

public:
  enum class Op : Mword
  {
    Map           = 0,
    Unmap         = 1,
    Cap_info      = 2,
    Add_ku_mem    = 3,
    Ldt_set_x86   = 0x11,
    Vgicc_map_arm = 0x12,
  };

  virtual int resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode);
  virtual void cleanup_vcpu(Context *ctxt, Vcpu_state *vcpu);

  /**
   * Map regions marked as eligible for eager mapping.
   *
   * MPU platforms provide only a limited number of MPU regions, so it is
   * essential to have contiguous regions in tasks. On the other hand we cannot
   * simply map too much because MPU regions must not overlap with the kernel.
   * Instead, bootstraps mark precisely the regions that shall be mapped
   * eagerly, and this function then maps them.
   */
  void map_all_segs(Mem_desc::Mem_type mt);
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
#include "global_data.h"

JDB_DEFINE_TYPENAME(Task, "\033[31mTask\033[m");

extern "C" [[noreturn]] void vcpu_resume(Trap_state *, Return_frame *sp)
   FIASCO_FASTCALL;

IMPLEMENT_DEFAULT
int
Task::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  // BAD: use the top-of the context stack area for the vcpu_resume
  // return, otherwise exceptions during return to user are very
  // ugly to handle.
  Trap_state *ts = reinterpret_cast<Trap_state *>(ctxt->regs() + 1) - 1;
  ctxt->copy_and_sanitize_trap_state(ts, &vcpu->_regs.s);

  if (user_mode)
    {
      ctxt->state_add_dirty(Thread_vcpu_user);
      vcpu->state |= Vcpu_state::F_traps;

      ctxt->vcpu_pv_switch_to_user(vcpu, true);
    }

  ctxt->space_ref()->user_mode(user_mode);
  switchin_context(ctxt->space());
  vcpu_resume(ts, ctxt->regs());
}

IMPLEMENT_DEFAULT
void
Task::cleanup_vcpu(Context *, Vcpu_state *)
{}

PUBLIC virtual
bool
Task::put() override
{ return dec_ref() == 0; }

/**
 * \see Task::alloc_ku_mem(L4_fpage)
 */
PRIVATE
int
Task::alloc_ku_mem_chunk(User_ptr<void> *u_addr, unsigned size, void **k_addr,
                         bool need_remote_tlb_flush)
{
  assert((size & (size - 1)) == 0);
  assert(size < Config::SUPERPAGE_SIZE);

  Kmem_alloc *const alloc = Kmem_alloc::allocator();
  void *p = alloc->q_alloc(ram_quota(), Bytes(size));

  if (EXPECT_FALSE(!p))
    return -L4_err::ENomem;

  memset(p, 0, size);

  Virt_addr base(p);
  Mem_space::Page_order page_order(Config::PAGE_SHIFT);

  // Need to use physical address on systems without MMU
  if constexpr (!Config::Have_mmu)
    *u_addr = User_ptr<void>(p);

  for (Virt_size i = Virt_size(0); i < Virt_size(size);
       i += Virt_size(1) << page_order)
    {
      Virt_addr kern_va = base + i;
      Virt_addr user_va = Virt_addr(u_addr->get()) + i;
      Mem_space::Phys_addr pa(pmem_to_phys(cxx::int_value<Virt_addr>(kern_va)));

      // must be valid physical address
      assert(pa != Mem_space::Phys_addr(~0UL));

      Mem_space::Status res =
        static_cast<Mem_space*>(this)->v_insert(pa, user_va, page_order,
            Mem_space::Attr::space_local(L4_fpage::Rights::URW()), true);

      switch (res)
        {
        case Mem_space::Insert_ok: break;
        case Mem_space::Insert_err_nomem:
          free_ku_mem_chunk(p, *u_addr, size, cxx::int_value<Virt_size>(i),
                            true, need_remote_tlb_flush);
          return -L4_err::ENomem;

        case Mem_space::Insert_err_exists:
          free_ku_mem_chunk(p, *u_addr, size, cxx::int_value<Virt_size>(i),
                            true, need_remote_tlb_flush);
          return -L4_err::EExists;

        default:
          printf("UTCB mapping failed: va=%p, ph=%p, res=%d\n",
                 static_cast<void *>(user_va), static_cast<void *>(kern_va), res);
          kdb_ke("BUG in utcb allocation");
          free_ku_mem_chunk(p, *u_addr, size, cxx::int_value<Virt_size>(i),
                            true, need_remote_tlb_flush);
          return 0;
        }
    }

  if constexpr (Mem_space::Need_insert_tlb_flush)
    {
      if (need_remote_tlb_flush)
        static_cast<Mem_space*>(this)->tlb_flush_all_cpus();
      else
        static_cast<Mem_space*>(this)->tlb_flush_current_cpu();
    }

  *k_addr = p;
  return 0;
}

/**
 * Allocate kernel user memory for this task.
 *
 * \pre Not thread-safe, the caller must ensure that no one else modifies the
 *      page table of the Task at the same time, for example by acquiring the
 *      existence lock or knowing that no one else has a reference to the Task
 *      object.
 * \pre If `need_remote_tlb_flush == true`, the cpu lock must not be held (as
 *      the remote TLB flush might do cross-cpu call) and the caller must ensure
 *      that the Task object does not get deleted.
 *
 * \param ku_area                Flexpage specifying the size and desired user
 *                               virtual address of the kernel user memory to
 *                               allocate.
 * \param need_remote_tlb_flush  Whether the Task might be active on another CPU
 *                               and thus requires a remote TLB flush after
 *                               allocating kernel user memory.
 */
PUBLIC
int
Task::alloc_ku_mem(L4_fpage *ku_area, bool need_remote_tlb_flush)
{
  // The limit comes from the kernel allocator (Buddy_alloc).
  if (ku_area->order() < Config::PAGE_SHIFT || ku_area->order() > 17)
    return -L4_err::EInval;

  Mword sz = 1UL << ku_area->order();

  if (ku_area->mem_address() > Virt_addr(Mem_layout::User_max - sz + 1))
    return -L4_err::EInval;

  Ku_mem *m = new (ram_quota()) Ku_mem();

  if (!m)
    return -L4_err::ENomem;

  User_ptr<void> u_addr(static_cast<void *>(ku_area->mem_address()));

  void *p = nullptr;
  if (int e = alloc_ku_mem_chunk(&u_addr, sz, &p, need_remote_tlb_flush))
    {
      m->free(ram_quota());
      return e;
    }

  m->u_addr = u_addr;
  m->k_addr = p;
  m->size = sz;
  *ku_area = L4_fpage::mem(reinterpret_cast<Mword>(u_addr.get()),
                           ku_area->order());

  _ku_mem.add(m, cas<cxx::S_list_item*>);

  return 0;
}

/**
 * \see Task::free_ku_mem()
 */
PRIVATE inline NOEXPORT
void
Task::free_ku_mem(Ku_mem *m, bool need_tlb_flush, bool need_remote_tlb_flush)
{
  free_ku_mem_chunk(m->k_addr, m->u_addr, m->size, m->size,
                    need_tlb_flush, need_remote_tlb_flush);
  m->free(ram_quota());
}

/**
 * \see Task::free_ku_mem()
 */
PRIVATE
void
Task::free_ku_mem_chunk(void *k_addr, User_ptr<void> u_addr, unsigned size,
                        unsigned mapped_size, bool need_tlb_flush,
                        bool need_remote_tlb_flush)
{
  Kmem_alloc * const alloc = Kmem_alloc::allocator();
  Mem_space::Page_order page_order(Config::PAGE_SHIFT);

  for (Virt_size i = Virt_size(0); i < Virt_size(mapped_size);
       i += Virt_size(1) << page_order)
    {
      Virt_addr user_va = Virt_addr(u_addr.get()) + i;
      static_cast<Mem_space*>(this)->v_delete(user_va, page_order,
                                              Page::Rights::FULL());
    }

  if (need_tlb_flush && mapped_size > 0)
    {
      if (need_remote_tlb_flush)
        static_cast<Mem_space*>(this)->tlb_flush_all_cpus();
      else
        static_cast<Mem_space*>(this)->tlb_flush_current_cpu();
    }

  alloc->q_free(ram_quota(), Bytes(size), k_addr);
}

/**
 * Free all kernel user memory of this Task.
 *
 * \pre Not thread-safe, the caller must ensure that no one else modifies the
 *      page table of the Task at the same time, for example by acquiring the
 *      existence lock or knowing that no one else has a reference to the Task
 *      object.
 * \pre If `need_remote_tlb_flush == true`, the cpu lock must not be held (as
 *      the remote TLB flush might do cross-cpu call) and the caller must ensure
 *      that the Task object does not get deleted.
 *
 * \param need_tlb_flush         Whether the Task requires a TLB flush after
 *                               freeing the kernel user memory.
 * \param need_remote_tlb_flush  Whether the Task might be active on another CPU
 *                               and thus requires a remote TLB flush after
 *                               freeing kernel user memory.
 */
PRIVATE
void
Task::free_ku_mem(bool need_tlb_flush, bool need_remote_tlb_flush)
{
  while (Ku_mem *m = _ku_mem.pop_front())
    free_ku_mem(m, need_tlb_flush, need_remote_tlb_flush);
}


/** Allocate resources for this task (e.g. UTCBs, page/cap directories).
 *  @ return true on success, false if not enough memory
 */
PUBLIC
bool
Task::initialize()
{
  if (!Space::initialize())
    return false;

  CNT_TASK_CREATE;

  return true;
}

/**
 * Create a normal Task.
 *
 * \pre \a parent must be valid and exist.
 */
PUBLIC explicit
Task::Task(Ram_quota *q, Caps c) : Space(q, c)
{
  // increment reference counter from zero
  inc_ref(true);
}

PUBLIC explicit
Task::Task(Ram_quota *q)
: Space(q, Caps::mem() | Caps::io() | Caps::obj() | Caps::threads())
{
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

static DEFINE_GLOBAL Global_data<Kmem_slab_t<Task>> _task_allocator("Task");

PRIVATE static
Task *Task::alloc(Ram_quota *q)
{
  return _task_allocator->q_new(q, q);
}

PUBLIC static
Task *Task::alloc(Ram_quota *q, Caps c)
{
  return _task_allocator->q_new(q, q, c);
}

PUBLIC //inline
void
Task::operator delete (Task *t, std::destroying_delete_t)
{
  Ram_quota *q = t->ram_quota();
  LOG_TRACE("Kobject delete", "del", current(), Log_destroy,
            l->id = t->dbg_id();
            l->obj = t;
            l->type = cxx::Typeid<Task>::get();
            l->ram = t->ram_quota()->current());

  t->~Task();
  _task_allocator->q_free(q, t);
}

PUBLIC template<typename TASK_TYPE, bool MUST_SYNC_KERNEL = true,
                int UTCB_AREA_MR = 0> static
TASK_TYPE * FIASCO_FLATTEN
Task::create(Ram_quota *q,
             L4_msg_tag t, Utcb const *u, Utcb *out,
             int *err, unsigned *words)
{
  static_assert(UTCB_AREA_MR == 0 || UTCB_AREA_MR >= 2,
                "invalid value for UTCB_AREA_MR");
  if (UTCB_AREA_MR >= 2 && EXPECT_FALSE(t.words() <= UTCB_AREA_MR))
    {
      *err = L4_err::EInval;
      return nullptr;
    }

  *err = L4_err::ENomem;
  cxx::unique_ptr<TASK_TYPE> v(TASK_TYPE::alloc(q));

  if (EXPECT_FALSE(!v))
    return nullptr;

  if (EXPECT_FALSE(!v->initialize()))
    return nullptr;

  if (MUST_SYNC_KERNEL && (v->sync_kernel() < 0))
    return nullptr;

  if (UTCB_AREA_MR >= 2)
    {
      L4_fpage utcb_area(access_once(&u->values[UTCB_AREA_MR]));
      if (utcb_area.is_valid())
        {
          // Task just newly created, no need for locking or remote TLB flush.
          int e = v->alloc_ku_mem(&utcb_area, false);
          if (e < 0)
            {
              *err = -e;
              return nullptr;
            }
          else
            {
              *words = 1;
              out->values[0] = utcb_area.raw();
            }
        }
    }

  return v.release();
}

PUBLIC template<typename TASK_TYPE, bool MUST_SYNC_KERNEL = false,
                int UTCB_AREA_MR = 0> static
Kobject_iface * FIASCO_FLATTEN
Task::generic_factory(Ram_quota *q, Space *,
                      L4_msg_tag t, Utcb const *u, Utcb *o,
                      int *err, unsigned *w)
{
  return create<TASK_TYPE, MUST_SYNC_KERNEL, UTCB_AREA_MR>(q, t, u, o, err, w);
}

/**
 * Shutdown the task.
 *
 * Currently:
 * -# Unbind and delete all contexts bound to this task.
 * -# Unmap everything from all spaces.
 * -# Delete child tasks.
 */
PUBLIC
void
Task::destroy(Kobjects_list &reap_list) override
{
  Kobject::destroy(reap_list);

  fpage_unmap(this, L4_fpage::all_spaces(L4_fpage::Rights::FULL()),
              L4_map_mask::full(), reap_list);
}

PRIVATE inline NOEXPORT
L4_msg_tag
Task::sys_map(L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb)
{
  LOG_TRACE("Task map", "map", ::current(), Log_map_unmap,
      l->id = dbg_id();
      l->map   = true;
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

  L4_error ret;

    {
      // Must be destroyed _after_ destruction of the lock guard below!
      Kobject::Reap_list reap_list;

      Ref_ptr<Task> from(_from);
      Ref_ptr<Task> self(this);
      // enforce lock order to prevent deadlocks.
      // always take lock from task with the lower memory address first
      Lock_guard_2<Helping_lock> guard;

      if (!guard.check_and_lock(&existence_lock, &from->existence_lock))
        return commit_result(-L4_err::EInval);

      cpu_lock.clear();
      ret = fpage_map(from.get(), sfp, this,
                      L4_fpage::all_spaces(), L4_snd_item(utcb->values[1]),
                      reap_list.list());
      cpu_lock.lock();
    }

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
  // Must be destroyed _after_ releasing the existence lock below!
  Kobject::Reap_list reap_list;
  unsigned words = f->tag().words();

  LOG_TRACE("Task unmap", "unm", ::current(), Log_map_unmap,
            l->id = dbg_id();
            l->map   = false;
            l->mask  = utcb->values[1];
            l->fpage = utcb->values[2]);

    {
      Ref_ptr<Task> self(this);

      auto guard = switch_lock_guard(existence_lock);
      if (!guard.is_valid())
        return commit_error(utcb, L4_error::Not_existent);

      cpu_lock.clear();

      L4_map_mask m(utcb->values[1]);

      for (unsigned i = 2; i < words; ++i)
        {
          Page::Flags const flushed
            = fpage_unmap(this, L4_fpage(utcb->values[i]), m, reap_list.list());

          utcb->values[i] = (utcb->values[i] & ~0xfUL)
                          | cxx::int_value<Page::Flags>(flushed);
        }
      cpu_lock.lock();
    }

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
    return commit_result(1);
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
Task::sys_add_ku_mem(Syscall_frame *f, Utcb *utcb, Utcb *out)
{
  if (EXPECT_FALSE(!(caps() & Task::Caps::kumem())))
    return commit_result(-L4_err::ENosys);

  // Acquire reference to ensure the task is not deleted while we try to acquire
  // its existence lock.
  Ref_ptr self(this);

  // Acquire existence lock to prevent concurrent modification of the Task's
  // page table.
  auto guard_task = switch_lock_guard(existence_lock);
  if (!guard_task.is_valid())
    return commit_error(utcb, L4_error::Not_existent);

  // alloc_ku_mem() must run with interrupts enabled (for potentially required
  // remote TLB flushes).
  auto guard_cpu = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  unsigned words = 0;
  unsigned const w = f->tag().words();
  for (unsigned i = 1; i < w; ++i)
    {
      L4_fpage ku_fp(utcb->values[i]);
      if (!ku_fp.is_valid() || !ku_fp.is_mempage())
        return commit_result(-L4_err::EInval);

      int e = alloc_ku_mem(&ku_fp, true);
      if (e < 0)
        return commit_result(e);

      if (out)
        out->values[words++] = ku_fp.raw();
    }

  return commit_result(0, words);
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
Task::invoke(L4_obj_ref self, L4_fpage::Rights rights, Syscall_frame *f, Utcb *utcb) override
{
  if (EXPECT_FALSE(f->tag().proto() != L4_msg_tag::Label_task))
    {
      f->tag(commit_result(-L4_err::EBadproto));
      return;
    }

  Utcb *out = self.have_recv() ? utcb : nullptr;
  L4_msg_tag tag;
  switch (Op{utcb->values[0]})
    {
    case Op::Map:
      tag = sys_map(rights, f, utcb);
      break;
    case Op::Unmap:
      tag = sys_unmap(f, utcb);
      break;
    case Op::Cap_info:
      tag = sys_cap_info(f, utcb);
      break;
    case Op::Add_ku_mem:
      tag = sys_add_ku_mem(f, utcb, out);
      break;
    default:
      L4_msg_tag arch_tag = f->tag();
      if (invoke_arch(arch_tag, utcb))
        tag = arch_tag;
      else
        tag = commit_result(-L4_err::ENosys);
      break;
    }

  if (self.have_recv() || tag.has_error())
    f->tag(tag);
}

namespace {

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(task_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_task,
                             &Task::generic_factory<Task, true, 2>);
}

}

IMPLEMENT_DEFAULT inline void
Task::map_all_segs(Mem_desc::Mem_type)
{}

PUBLIC inline
Task::~Task()
{
  // Task, and thus also its page table, is no longer used anywhere at this
  // point, so no not necessary to flush the freed kernel user memory mappings
  // from the TLB.
  free_ku_mem(false, false);
}


// ---------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Task
{
private:
  struct Log_map_unmap : public Tb_entry
  {
    Mword id;
    Mword mask;
    Mword fpage;
    bool  map;
    void print(String_buffer *buf) const;
  };
  static_assert(sizeof(Log_map_unmap) <= Tb_entry::Tb_entry_size);
};

// ---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

IMPLEMENT
void
Task::Log_map_unmap::print(String_buffer *buf) const
{
  L4_fpage fp(fpage);
  buf->printf("task=[%c:%lx] %s=%lx fpage=[%u/",
              map ? 'M' : 'U', id,
              map ? "snd_base" : "mask", mask, fp.order().get());
  switch (fp.type())
    {
    case L4_fpage::Special:
      buf->printf("spc] fpage=%lx", fpage);
      break;
    case L4_fpage::Memory:
      buf->printf("mem] addr=%lx", cxx::int_value<Virt_addr>(fp.mem_address()));
      break;
    case L4_fpage::Io:
      buf->printf("io] port=%lx", cxx::int_value<Port_number>(fp.io_address()));
      break;
    case L4_fpage::Obj:
      buf->printf("obj] cap=C:%lx", cxx::int_value<Cap_index>(fp.obj_index()));
      break;
    default:
      buf->printf("???] fpage=%lx", fpage);
      break;
    }
}
