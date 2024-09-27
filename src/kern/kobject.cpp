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
 * Helper class for maintaining a linked list of #Kobject objects.
 *
 * The list is a singly-linked list with a head. The head is a reference to
 * the location where the pointer to the list item is stored.
 */
class Kobjects_list
{
public:
  using Ptr = Kobject *;

  Kobjects_list() = delete;

  /**
   * Construct the list.
   *
   * The list is constructed as empty, i.e. nullptr is stored to the head of
   * the list.
   *
   * \param head  Head of the list, i.e. a reference to the location where
   *              the pointer to the first item is stored.
   */
  Kobjects_list(Ptr &head) : _head(&head)
  {
    *_head = nullptr;
  }

  /**
   * Reset the list into an empty state.
   *
   * The list is reset to an empty state, i.e. nullptr is stored to the head
   * of the list.
   *
   * \param head  Head of the list, i.e. a reference to the location where
   *              the pointer to the first item is stored.
   */
  void reset(Ptr &head)
  {
    _head = &head;
    *_head = nullptr;
  }

  /**
   * Push an item into the list.
   *
   * Store an item into the head and set a new head of the list. This
   * effectively pushes the item into the linked list and creates the space
   * for the next item.
   *
   * \param item      Item to be stored in the current head of the list.
   * \param new_head  New head of the list, i.e. a reference to the location
   *                  where the pointer to the next item is stored. This new
   *                  head is initially set to nullptr.
   */
  void push(Ptr item, Ptr &new_head)
  {
    // Set the current item.
    *_head = item;

    // Prepare new head for the next item.
    _head = &new_head;
    *_head = nullptr;
  }

private:
  Ptr *_head;
};

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
   *
   * \note Be careful about the following case: An object O is part of the reap
   *       list RL. If the current thread is the holder of the existence lock of
   *       O, the destructor of RL must **not** be called before the existence
   *       lock of O is released. This is usually ensured by declaring RL before
   *       acquiring the existence lock -- so that RL is destroyed after the
   *       existence lock of O is released.
   */
  class Reap_list
  {
  private:
    Kobjects_list::Ptr _first = nullptr;
    Kobjects_list _reap_list;

  public:
    Reap_list() : _reap_list(_first) {}
    ~Reap_list() { del(); }

    Kobjects_list &list() { return _reap_list; }
    void reset() { _reap_list.reset(_first); }
    bool empty() const { return _first == nullptr; }

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
  Kobjects_list::Ptr _next_to_reap = nullptr;

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
Kobject::initiate_deletion(Kobjects_list &reap_list) override
{
  existence_lock.invalidate();
  reap_list.push(this, _next_to_reap);
}

PUBLIC virtual
void
Kobject::destroy(Kobjects_list &)
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
  for (Kobject *obj = _first; obj; obj = obj->_next_to_reap)
    obj->destroy(list());
}

IMPLEMENT inline
void
Kobject::Reap_list::del_2()
{
  for (Kobject *obj = _first; obj;)
    {
      Kobject *current = obj;
      obj = obj->_next_to_reap;

      if (current->put())
        delete current;
    }

  reset();
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
