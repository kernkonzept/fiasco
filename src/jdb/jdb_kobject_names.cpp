//-----------------------------------------------------------------------
INTERFACE:

#include "config.h"
#include "jdb_kobject.h"
#include "l4_types.h"
#include "initcalls.h"


class Jdb_kobject_name : public Jdb_kobject_extension
{
public:
  static char const *const static_type;
  virtual char const *type() const { return static_type; }

  ~Jdb_kobject_name() {}

  void *operator new (size_t) throw();
  void operator delete (void *);

private:

  char _name[16];

  static Jdb_kobject_name *_names;
};

//-----------------------------------------------------------------------
IMPLEMENTATION:

#include <cstdio>

#include <feature.h>
#include "context.h"
#include "kmem_alloc.h"
#include "minmax.h"
#include "panic.h"
#include "space.h"
#include "thread.h"
#include "static_init.h"



enum
{
  Name_buffer_size = 8192,
  Name_entries = Name_buffer_size / sizeof(Jdb_kobject_name),
};


char const *const Jdb_kobject_name::static_type = "Jdb_kobject_names";
Jdb_kobject_name *Jdb_kobject_name::_names;


PUBLIC
int
Jdb_kobject_name::max_len()
{ return sizeof(_name); }

PUBLIC
Jdb_kobject_name::Jdb_kobject_name()
{ _name[0] = 0; }

static Spin_lock<> allocator_lock;

IMPLEMENT
void *
Jdb_kobject_name::operator new (size_t) throw()
{
  Jdb_kobject_name *n = _names;
  while (1)
    {
      void **o = reinterpret_cast<void**>(n);
      if (!*o)
	{
	  auto g = lock_guard(allocator_lock);
	  if (!*o)
	    {
	      *o = (void*)10;
	      return n;
	    }
	}

      ++n;

      if ((n - _names) >= Name_entries)
	return 0;
    }
}

IMPLEMENT
void
Jdb_kobject_name::operator delete (void *p)
{
  auto g = lock_guard(allocator_lock);
  void **o = reinterpret_cast<void**>(p);
  *o = 0;
}

PUBLIC
void
Jdb_kobject_name::name(char const *name, int size)
{
  int i = 0;
  if (size > max_len())
    size = max_len();
  for (; name[i] && i < size; ++i)
    _name[i] = name[i];

  for (; i < max_len(); ++i)
    _name[i] = 0;
}

PUBLIC inline
char const *
Jdb_kobject_name::name() const
{ return _name; }

PUBLIC inline
char *
Jdb_kobject_name::name()
{ return _name; }

class Jdb_name_hdl : public Jdb_kobject_handler
{
public:
  virtual bool show_kobject(Kobject_common *, int) { return true; }
  virtual ~Jdb_name_hdl() {}
};

PUBLIC
void
Jdb_name_hdl::show_kobject_short(String_buffer *buf, Kobject_common *o)
{
  Jdb_kobject_name *ex
    = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o);

  if (ex)
    buf->printf(" {%-*.*s}", ex->max_len(), ex->max_len(), ex->name());
}

PUBLIC
bool
Jdb_name_hdl::invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb)
{
  switch (utcb->values[0])
    {
    case Op_set_name:
        {
          bool enqueue = false;
          Jdb_kobject_name *ne;
          ne = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(o);
          if (!ne)
            {
              ne = new Jdb_kobject_name();
              if (!ne)
                {
                  f->tag(Kobject_iface::commit_result(-L4_err::ENomem));
                  return true;
                }
              enqueue = true;
            }

          if (f->tag().words() > 0)
            ne->name(reinterpret_cast<char const *>(&utcb->values[1]),
                     (f->tag().words() - 1) * sizeof(Mword));
          if (enqueue)
            o->dbg_info()->_jdb_data.add(ne);
          f->tag(Kobject_iface::commit_result(0));
          return true;
        }
    case Op_get_name:
        {
          Kobject_dbg::Iterator o = Kobject_dbg::id_to_obj(utcb->values[1]);
          if (o == Kobject_dbg::end())
            {
              f->tag(Kobject_iface::commit_result(-L4_err::ENoent));
              return true;
            }
          Jdb_kobject_name *n = Jdb_kobject_extension::find_extension<Jdb_kobject_name>(Kobject::from_dbg(o));
          if (!n)
            {
              f->tag(Kobject_iface::commit_result(-L4_err::ENoent));
              return true;
            }

          unsigned l = min<unsigned>(n->max_len(), sizeof(utcb->values));
          char *dst = reinterpret_cast<char *>(utcb->values);
          strncpy(dst, n->name(), l);
          dst[l - 1] = 0;

          f->tag(Kobject_iface::commit_result(0));
          return true;
        }
    }
  return false;
}

PUBLIC static FIASCO_INIT
void
Jdb_kobject_name::init()
{
  _names = (Jdb_kobject_name*)Kmem_alloc::allocator()->unaligned_alloc(Name_buffer_size);
  if (!_names)
    panic("No memory for thread names");

  for (int i=0; i<Name_entries; i++)
    *reinterpret_cast<unsigned long*>(_names + i) = 0;

  static Jdb_name_hdl hdl;
  Jdb_kobject::module()->register_handler(&hdl);
}


STATIC_INITIALIZE(Jdb_kobject_name);

