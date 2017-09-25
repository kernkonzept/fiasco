//----------------------------------------------------------------------------
INTERFACE[debug]:

#include "spin_lock.h"
#include "lock_guard.h"
#include <cxx/dlist>
#include <cxx/hlist>
#include <cxx/dyn_cast>

struct Kobject_typeinfo_name
{
  cxx::Type_info const *type;
  char const *name;
};

#define JDB_DEFINE_TYPENAME(type, name) \
  static __attribute__((used, section(".debug.jdb.typeinfo_table"))) \
  Kobject_typeinfo_name const typeinfo_name__ ## type ## __entry =   \
    { cxx::Typeid<type>::get(), name }

class Kobject_dbg : public cxx::D_list_item
{
  friend class Jdb_kobject;
  friend class Jdb_kobject_list;
  friend class Jdb_mapdb;

public:
  class Dbg_extension : public cxx::H_list_item
  {
  public:
    virtual ~Dbg_extension() = 0;
  };

public:
  typedef cxx::H_list<Dbg_extension> Dbg_ext_list;
  Dbg_ext_list _jdb_data;

private:
  Mword _dbg_id;

public:
  Mword dbg_id() const { return _dbg_id; }

  virtual cxx::_dyn::Type _cxx_dyn_type() const = 0;
  virtual ~Kobject_dbg() = 0;


  typedef cxx::D_list<Kobject_dbg> Kobject_list;
  typedef Kobject_list::Iterator Iterator;
  typedef Kobject_list::Const_iterator Const_iterator;

  static Spin_lock<> _kobjects_lock;
  static Kobject_list _kobjects;

  static Iterator begin() { return _kobjects.begin(); }
  static Iterator end() { return _kobjects.end(); }

private:
  static unsigned long _next_dbg_id;
};

//----------------------------------------------------------------------------
INTERFACE[!debug]:

#define JDB_DEFINE_TYPENAME(type, name)

class Kobject_dbg
{
public:
  typedef unsigned long Iterator;
};

//----------------------------------------------------------------------------
IMPLEMENTATION[debug]:
#include "static_init.h"
Spin_lock<> Kobject_dbg::_kobjects_lock;
Kobject_dbg::Kobject_list Kobject_dbg::_kobjects INIT_PRIORITY(101);
unsigned long Kobject_dbg::_next_dbg_id;

IMPLEMENT inline Kobject_dbg::Dbg_extension::~Dbg_extension() {}

PUBLIC static
Kobject_dbg::Iterator
Kobject_dbg::pointer_to_obj(void const *p)
{
  for (Iterator l = _kobjects.begin(); l != _kobjects.end(); ++l)
    {
      auto ti = l->_cxx_dyn_type();
      Mword a = (Mword)ti.base;
      if (a <= Mword(p) && Mword(p) < (a + ti.type->size))
        return l;
    }
  return _kobjects.end();
}

PUBLIC static
unsigned long
Kobject_dbg::pointer_to_id(void const *p)
{
  Iterator o = pointer_to_obj(p);
  if (o != _kobjects.end())
    return o->dbg_id();
  return ~0UL;
}

PUBLIC static
bool
Kobject_dbg::is_kobj(void const *o)
{
  return pointer_to_obj(o) != _kobjects.end();
}

PUBLIC static
Kobject_dbg::Iterator
Kobject_dbg::id_to_obj(unsigned long id)
{
  for (Iterator l = _kobjects.begin(); l != _kobjects.end(); ++l)
    {
      if (l->dbg_id() == id)
	return l;
    }
  return end();
}

PUBLIC static
unsigned long
Kobject_dbg::obj_to_id(void const *o)
{
  return pointer_to_id(o);
}


PROTECTED
Kobject_dbg::Kobject_dbg()
{
  auto guard = lock_guard(_kobjects_lock);

  _dbg_id = _next_dbg_id++;
  _kobjects.push_back(this);
}

IMPLEMENT inline
Kobject_dbg::~Kobject_dbg()
{
    {
      auto guard = lock_guard(_kobjects_lock);
      _kobjects.remove(this);
    }

  while (Dbg_extension *ex = _jdb_data.front())
    delete ex;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

PUBLIC inline
unsigned long
Kobject_dbg::dbg_id() const
{ return 0; }

PUBLIC static inline
unsigned long
Kobject_dbg::dbg_id(void const *)
{ return ~0UL; }

PUBLIC static inline
Kobject_dbg::Iterator
Kobject_dbg::pointer_to_obj(void const *)
{ return 0; }

PUBLIC static inline
unsigned long
Kobject_dbg::pointer_to_id(void const *)
{ return ~0UL; }

PUBLIC static
bool
Kobject_dbg::is_kobj(void const *)
{ return false; }

PUBLIC static
Kobject_dbg::Iterator
Kobject_dbg::id_to_obj(unsigned long)
{ return 0; }

PUBLIC static
unsigned long
Kobject_dbg::obj_to_id(void const *)
{ return ~0UL; }
