INTERFACE:

#include "lock.h"
#include "obj_space.h"
#include <cxx/hlist>


class Kobject_mappable
{
private:
  friend class Kobject_mapdb;
  friend class Jdb_mapdb;

  Obj::Mapping::List _root;
  Smword _cnt;
  Lock _lock;

public:
  Kobject_mappable() : _cnt(0) {}
  bool no_mappings() const { return _root.empty(); }

  /**
   * Insert the root mapping of an object.
   */
  template<typename M>
  bool insert(void *, Space *, M &m)
  {
    m._c->put_as_root();
    _root.add(m._c);
    _cnt = 1;
    return true;
  }

  Smword cap_ref_cnt() const { return _cnt; }
};


//----------------------------------------------------------------------------
INTERFACE:

#include "context.h"
#include "kobject_dbg.h"
#include "kobject_iface.h"
#include "l4_error.h"
#include "rcupdate.h"
#include "space.h"

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

  class Reap_list
  {
  private:
    Kobject *_h;
    Kobject **_t;

  public:
    Reap_list() : _h(0), _t(&_h) {}
    ~Reap_list() { del(); }
    Kobject ***list() { return &_t; }
    void del();
  };

  using Kobject_dbg::dbg_id;

  Lock existence_lock;

public:
  Kobject *_next_to_reap;

public:
  enum Op {
    O_dec_refcnt = 0,
  };

};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include "logdefs.h"
#include "l4_buf_iter.h"
#include "lock_guard.h"


PUBLIC bool  Kobject::is_local(Space *) const { return false; }
PUBLIC Mword Kobject::obj_id() const { return ~0UL; }
PUBLIC virtual bool  Kobject::put() { return true; }
PUBLIC inline Kobject_mappable *Kobject::map_root() { return this; }

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
Kobject::initiate_deletion(Kobject ***reap_list)
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

PUBLIC
L4_msg_tag
Kobject::kobject_invoke(L4_obj_ref, L4_fpage::Rights /*rights*/,
                        Syscall_frame *f,
                        Utcb const *in, Utcb *out)
{
  L4_msg_tag tag = f->tag();

  if (EXPECT_FALSE(tag.words() < 1))
    return Kobject_iface::commit_result(-L4_err::EInval);

  switch (in->values[0])
    {
    case O_dec_refcnt:
      return sys_dec_refcnt(tag, in, out);
    default:
      return Kobject_iface::commit_result(-L4_err::ENosys);
    }

}


IMPLEMENT
void
Kobject::Reap_list::del()
{
  if (EXPECT_TRUE(!_h))
    return;

  for (Kobject *reap = _h; reap; reap = reap->_next_to_reap)
    reap->destroy(list());

  current()->rcu_wait();

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
};

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

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
Kobject::dbg_info() const
{ return const_cast<Kobject*>(this); }

IMPLEMENT
void
Kobject::Log_destroy::print(String_buffer *buf) const
{
  buf->printf("obj=%lx [%p] (%p) ram=%lx", id, type, obj, ram);
}
