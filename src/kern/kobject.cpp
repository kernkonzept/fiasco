INTERFACE:

#include "lock.h"
#include "obj_space.h"
#include <cxx/hlist>


class Kobject_mappable
{
private:
  friend class Kobject_mapdb;
  friend class Jdb_mapdb;
  friend class Obj_mapdb_test;

  Obj::Mapping::List _root;
  Smword _cnt;
  Lock _lock;

public:
  Kobject_mappable() : _cnt(0) {}
  bool no_mappings() const { return _root.empty(); }

  /**
   * Insert the root mapping of an object.
   */
  void insert_root_mapping(Obj::Cap_addr &m)
  {
    m._c->put_as_root();
    _root.add(m._c);
    _cnt = 1;
  }

private:
  void invalidate_mappings()
  {
    assert(_lock.test());

    for (auto const &&i: _root)
      {
        Obj::Entry *e = static_cast<Obj::Entry*>(i);
        if (e->is_ref_counted())
          --_cnt;
        e->invalidate();
      }

    _root.clear();
  }
};


//----------------------------------------------------------------------------
INTERFACE:

#include "context.h"
#include "kobject_dbg.h"
#include "kobject_iface.h"
#include "l4_error.h"
#include "rcupdate.h"
#include "space.h"

/**
 * Basic kernel object.
 *
 * This is the base class for all kernel objects that are exposed through
 * capabilities.
 *
 * Kernel objects have a specific life-cycle management that is based
 * on (a) kernel external visibility through capabilities and (b) kernel
 * internal references among kernel objects.
 *
 * If there are no more capabilities to a kernel object, due to an unmap
 * (either of the last capability, or with delete flag and delete rights),
 * the kernel runs the 2-phase destruction protocol for kernel objects
 * (implemented in Kobject::Reap_list::del()):
 * - Release the CPU lock.
 * - Call Kobject::destroy(). Mark this object as invalid. When this function
 *   returns, the object cannot be longer referenced. One object reference is
 *   still maintained to prevent the final removal before all other objects
 *   realized that the object vanished.
 * - Block for an RCU grace period.
 * - Call Kobject::put(). Release the final reference to the object preparing
 *   the object deletion.
 * - Re-acquire the CPU lock.
 *
 * If Kobject::put() returns true, the kernel object is to be deleted.
 * If there are any other non-capability references to a kernel object,
 * Kobject::put() *must* return `false` and an object-specific MP-safe
 * mechanism for object deletions has to take over.
 *
 * Kobject::destroy() and Kobject::put() are called exactly once as a direct
 * consequence of unmapping. Usually, during the lifetime of a `Kobject`,
 * there are no additional calls to those functions; however, object-specific
 * reference handling may allow or require additional calls.
 */
class Kobject :
  public cxx::Dyn_castable<Kobject, Kobject_iface>,
  private Kobject_mappable,
  private Kobject_dbg
{
  template<typename T>
  friend class Map_traits;

private:
  template<typename T>
  class Tconv {};

  template<typename T>
  class Tconv<T*> { public: typedef T Base; };

public:
  using Dyn_castable<Kobject, Kobject_iface>::_cxx_dyn_type;

  /**
   * Automatic deletion of a list of kernel objects from the destructor of
   * this class.
   *
   * \note The destructor temporarily releases the CPU lock while objects are
   *       destroyed like described in Kobject.
   */
  class Reap_list
  {
  private:
    Kobject *_h;
    Kobject **_t;

  public:
    Reap_list() : _h(0), _t(&_h) {}
    ~Reap_list() { del(); }
    Kobject ***list() { return &_t; }
    bool empty() const { return _h == nullptr; }
    void del_1();
    void del_2();

  private:
    /**
     * Delete kernel objects without capability references.
     */
    void del()
    {
      if (EXPECT_TRUE(empty()))
        return;

      auto c_lock = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
      del_1();
      current()->rcu_wait();
      del_2();
    }
  };

  using Kobject_dbg::dbg_id;

  /**
   * \warning Acquiring the existence lock of a kernel object is a potential
   *          preemption point! You must ensure that the kernel object cannot be
   *          deleted before the existence lock can be taken, for example by
   *          holding a counted reference to it.
   */
  Lock existence_lock;

private:
  Kobject *_next_to_reap;

public:
  enum class Op : Mword
  {
    Dec_refcnt = 0,
  };

};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "logdefs.h"
#include "l4_buf_iter.h"
#include "lock_guard.h"


PUBLIC bool  Kobject::is_local(Space *) const override { return false; }
PUBLIC Mword Kobject::obj_id() const override { return ~0UL; }
PUBLIC virtual bool  Kobject::put() { return true; }
PUBLIC inline Kobject_mappable *Kobject::map_root() override { return this; }

PUBLIC inline NEEDS["lock_guard.h"]
Smword
Kobject_mappable::dec_cap_refcnt(Smword diff)
{
  auto g = lock_guard(_lock);
  _cnt -= diff;
  return _cnt;
}

PUBLIC
void
Kobject::initiate_deletion(Kobject ***reap_list) override
{
  existence_lock.invalidate();

  _next_to_reap = 0;
  **reap_list = this;
  *reap_list = &_next_to_reap;
}

PUBLIC virtual
void
Kobject::destroy(Kobject ***)
{
  LOG_TRACE("Kobject destroy", "des", current(), Log_destroy,
      l->id = dbg_id();
      l->obj = this;
      l->type = cxx::dyn_typeid(this));
  existence_lock.wait_free();
}

PUBLIC virtual
Kobject::~Kobject()
{
  LOG_TRACE("Kobject delete (generic)", "del", current(), Log_destroy,
      l->id = dbg_id();
      l->obj = this;
      l->type = 0);
}


PRIVATE inline NOEXPORT
L4_msg_tag
Kobject::sys_dec_refcnt(L4_msg_tag tag, Utcb const *in, Utcb *out)
{
  if (tag.words() < 2)
    return Kobject_iface::commit_result(-L4_err::EInval);

  Smword diff = in->values[1];
  out->values[0] = dec_cap_refcnt(diff);
  return Kobject_iface::commit_result(0, 1);
}

/**
 * \pre tag.words() >= 1
 */
PUBLIC
L4_msg_tag
Kobject::kobject_invoke(L4_obj_ref, L4_fpage::Rights /*rights*/,
                        Syscall_frame *f,
                        Utcb const *in, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  // The only caller, Ipc_gate_ctl::kinvoke(), tests for tag.words() >= 1.

  switch (Op{in->values[0]})
    {
    case Op::Dec_refcnt:
      return sys_dec_refcnt(tag, in, out);
    default:
      return Kobject_iface::commit_result(-L4_err::ENosys);
    }

}


IMPLEMENT inline
void
Kobject::Reap_list::del_1()
{
  for (Kobject *reap = _h; reap; reap = reap->_next_to_reap)
    reap->destroy(list());
}

IMPLEMENT inline
void
Kobject::Reap_list::del_2()
{
  for (Kobject *reap = _h; reap;)
    {
      Kobject *d = reap;
      reap = reap->_next_to_reap;
      if (d->put())
        delete d;
    }

  _h = 0;
  _t = &_h;
}


//---------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Kobject
{
protected:
  struct Log_destroy : public Tb_entry
  {
    Kobject    *obj;
    Mword       id;
    cxx::Type_info const *type;
    Mword       ram;
    void print(String_buffer *buf) const;
  };
  static_assert(sizeof(Log_destroy) <= Tb_entry::Tb_entry_size);
};

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

IMPLEMENT
void
Kobject::Log_destroy::print(String_buffer *buf) const
{
  buf->printf("obj=%lx [%p] (%p) ram=%lx", id, static_cast<void const *>(type),
              static_cast<void *>(obj), ram);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [rt_dbg]:

PUBLIC static inline
Kobject *
Kobject::from_dbg(Kobject_dbg *d)
{ return static_cast<Kobject*>(d); }

PUBLIC static inline
Kobject *
Kobject::from_dbg(Kobject_dbg::Iterator const &d)
{
  if (d != Kobject_dbg::end())
    return static_cast<Kobject*>(*d);
  return 0;
}

PUBLIC
Kobject_dbg *
Kobject::dbg_info() const override
{ return const_cast<Kobject*>(this); }
