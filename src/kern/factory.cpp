INTERFACE:

#include "fiasco_defs.h"
#include "ram_quota.h"
#include "slab_cache.h"
#include "kmem_slab.h"
#include "kobject_helper.h"

class Factory : public Ram_quota, public Kobject_h<Factory>
{
  friend struct Factory_test;

public:
  using Ram_quota::alloc;
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "static_init.h"
#include "l4_buf_iter.h"
#include "l4_types.h"
#include "irq.h" // for backward compat
#include "map_util.h"
#include "logdefs.h"
#include "entry_frame.h"
#include "task.h"
#include "global_data.h"

JDB_DEFINE_TYPENAME(Factory, "\033[33;1mFactory\033[m");

static DEFINE_GLOBAL_PRIO(ROOT_FACTORY_INIT_PRIO)
Global_data<Factory> _root_factory;

PUBLIC inline
Factory::Factory()
  : Ram_quota()
{}

PRIVATE inline
Factory::Factory(Ram_quota *q, Mword max)
  : Ram_quota(q, max)
{}


static DEFINE_GLOBAL
Global_data<Kmem_slab_t<Factory>> _factory_allocator("Factory");

PRIVATE static
void *
Factory::alloc()
{ return _factory_allocator->alloc(); }

PRIVATE static
void
Factory::free(void *f)
{ _factory_allocator->free(f); }

PUBLIC static inline
Factory * FIASCO_PURE
Factory::root()
{ return nonull_static_cast<Factory*>(Ram_quota::root.unwrap()); }

PUBLIC
void
Factory::destroy(Kobjects_list &reap_list) override
{
  Kobject::destroy(reap_list);
  take_and_invalidate();
}

PUBLIC inline
bool
Factory::put() override
{
  return Ram_quota::put();
}

PUBLIC inline NEEDS[Factory::Factory]
Factory *
Factory::create_factory(Mword max)
{
  if (!check_max(max))
    return nullptr;

  Auto_quota<Ram_quota> q(this, sizeof(Factory) + max);
  if (EXPECT_FALSE(!q))
    return nullptr;

  void *nq = alloc();
  if (EXPECT_FALSE(!nq))
    return nullptr;

  q.release();
  return new (nq) Factory(this, max);
}

PUBLIC
void Factory::operator delete (Factory *f, std::destroying_delete_t)
{
  LOG_TRACE("Factory delete", "fa del", ::current(), Tb_entry_empty, {});

  Ram_quota *p = f->parent();
  auto limit = f->limit();

  f->~Factory();
  free(f);
  if (p)
    p->free(sizeof(Factory) + limit);
}

PRIVATE
L4_msg_tag
Factory::map_obj(Kobject_iface *o, Cap_index cap, Task *_c_space,
                 Obj_space *o_space, Utcb const *utcb, unsigned words)
{
  // must be before the lock guard
  Ref_ptr<Task> c_space(_c_space);
  // Must be destroyed _after_ releasing the existence lock below!
  Reap_list reap_list;

  // Take the existence_lock for synchronizing maps -- kind of coarse-grained.
  auto space_lock_guard = switch_lock_guard(c_space->existence_lock);
  if (!space_lock_guard.is_valid())
    {
      delete o;
      return commit_error(utcb, L4_error(L4_error::Overflow, L4_error::Rcv));
    }

  if (!map_obj_initially(o, o_space, c_space.get(), cap, reap_list.list()))
    {
      delete o;
      return commit_result(-L4_err::ENomem);
    }

  // return a tag with one typed item for the returned capability
  return commit_result(0, words, 1);
}

PUBLIC
L4_msg_tag
Factory::kinvoke(L4_obj_ref ref, L4_fpage::Rights rights, Syscall_frame *f,
                 Utcb const *utcb, Utcb *utcb_out)
{
  Context *const c_thread = ::current();
  Task *const c_space = static_cast<Task*>(c_thread->space());

  L4_msg_tag tag = f->tag();
  if (!Ko::check_basics(&tag, rights, L4_msg_tag::Label_factory,
                        L4_fpage::Rights::CS()))
    return tag;

  if (EXPECT_FALSE(!ref.have_recv()))
    return commit_result(0);

  L4_fpage buffer(0);

    {
      L4_buf_iter buf(utcb, utcb->buf_desc.obj());
      L4_buf_iter::Item const *const b = buf.get();
      if (EXPECT_FALSE(b->b.is_void()
                       || b->b.type() != L4_msg_item::Map))
        return commit_error(utcb, L4_error(L4_error::Overflow, L4_error::Rcv));

      buffer = L4_fpage(b->d);
    }

  if (EXPECT_FALSE(!buffer.is_objpage()))
    return commit_error(utcb, L4_error(L4_error::Overflow, L4_error::Rcv));

  Kobject_iface *new_o;
  int err = L4_err::ENomem;
  unsigned words = 0;

  auto cpu_lock_guard = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  long label = static_cast<long>(access_once(utcb->values + 0));
  new_o = Kobject_iface::manufacture(label, this, c_space, f->tag(), utcb,
                                     utcb_out, &err, &words);

  LOG_TRACE("Kobject create", "new", ::current(), Log_entry,
    l->op = label;
    l->buffer = buffer.obj_index();
    l->id = dbg_info()->dbg_id();
    l->ram = current();
    l->newo = new_o ? new_o->dbg_info()->dbg_id() : ~0);

  if (new_o)
    {
      utcb_out->values[words] = (0 << 6) | (L4_fpage::Obj << 4) | L4_msg_item::Map;
      return map_obj(new_o, buffer.obj_index(), c_space, c_space, utcb, words);
    }
  else
    return commit_result(-err);
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
factory_factory(Ram_quota *q, Space *,
                L4_msg_tag tag, Utcb const *u, Utcb *,
                int *err, unsigned *)
{
  if (EXPECT_FALSE(tag.words() < 3))
    {
      *err = L4_err::EMsgtooshort;
      return nullptr;
    }
  else if (EXPECT_FALSE(tag.words() > 3 || tag.items() > 0))
    {
      *err = L4_err::EMsgtoolong;
      return nullptr;
    }

  *err = L4_err::ENomem;
  return static_cast<Factory*>(q)->create_factory(u->values[2]);
}

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(factory_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_factory, factory_factory);
}

}

// ------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Factory
{
private:
  struct Log_entry : public Tb_entry
  {
    Smword op;
    Cap_index buffer;
    Mword id;
    Mword ram;
    Mword newo;
    void print(String_buffer *buf) const;
  };
  static_assert(sizeof(Log_entry) <= Tb_entry::Tb_entry_size);
};

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

IMPLEMENT
void
Factory::Log_entry::print(String_buffer *buf) const
{
  static char const *const ops[] =
  {
    /*   0 */ "gate", "irq", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    /*  -8 */ nullptr, nullptr, nullptr, "task", "thread", nullptr, nullptr, "factory",
    /* -16 */ "vm", "dmaspace", "irqsender", nullptr, "sem"
  };
  buf->printf("factory=%lx ", id);
  if (op <= 0 && op > -int{cxx::size(ops)})
    buf->printf("[%s]", ops[-op] ? ops[-op] : "(nan)");
  else
    buf->printf("[invalid proto %ld]", op);
  buf->printf(" new=%lx cap=[C:%lx] ram=%lx",
              newo, cxx::int_value<Cap_index>(buffer), ram);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [test_support_code]:

PRIVATE static
Slab_cache *
Factory::allocator()
{ return _factory_allocator->slab(); }
