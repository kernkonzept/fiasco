INTERFACE:

#include "jdb_module.h"
#include "jdb_list.h"
#include "kobject.h"
#include "string_buffer.h"

#include <cxx/slist>

class Kobject;
class Jdb_kobject_handler;

class Jdb_kobject : public Jdb_module
{
public:
  typedef cxx::S_list_tail<Jdb_kobject_handler> Handler_list;
  typedef Handler_list::Const_iterator Handler_iter;

  Jdb_kobject();

  Handler_list handlers;
  Handler_list global_handlers;

private:
  static void *kobjp;
};


class Jdb_kobject_handler : public cxx::S_list_item
{
  friend class Jdb_kobject;

public:
  template<typename T>
  Jdb_kobject_handler(T const *) : kobj_type(cxx::Typeid<T>::get()) {}
  Jdb_kobject_handler() : kobj_type(0) {}
  cxx::Type_info const *kobj_type;
  virtual bool show_kobject(Kobject_common *o, int level) = 0;
  virtual void show_kobject_short(String_buffer *, Kobject_common *) {}
  virtual Kobject_common *follow_link(Kobject_common *o) { return o; }
  virtual ~Jdb_kobject_handler() {}
  virtual bool invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb);
  virtual bool handle_key(Kobject_common *, int /*keycode*/) { return false; }
  virtual Kobject *parent(Kobject_common *) { return 0; }
  char const *kobject_type(Kobject_common *o) const
  { return _kobject_type(o); }

  static char const *_kobject_type(Kobject_common *o)
  {
    extern Kobject_typeinfo_name const _jdb_typeinfo_table[];
    extern Kobject_typeinfo_name const _jdb_typeinfo_table_end[];

    for (Kobject_typeinfo_name const *t = _jdb_typeinfo_table;
        t != _jdb_typeinfo_table_end; ++t)
      if (t->type == cxx::dyn_typeid(o))
        return t->name;

    return cxx::dyn_typeid(o)->name;
  }

  bool is_global() const { return !kobj_type; }

protected:
  enum {
    Op_set_name         = 0,
    Op_global_id        = 1,
    Op_kobj_to_id       = 2,
    Op_query_log_typeid = 3,
    Op_switch_log       = 4,
    Op_get_name         = 5,
    Op_query_log_name   = 6,
  };
};

class Jdb_kobject_extension : public Kobject_dbg::Dbg_extension
{
public:
  virtual ~Jdb_kobject_extension() {}
  virtual char const *type() const = 0;
};

class Jdb_kobject_list : public Jdb_list
{
public:
  typedef bool Filter_func(Kobject_common const *);

  struct Mode : cxx::S_list_item
  {
    char const *name;
    Filter_func *filter;
    typedef cxx::S_list_bss<Mode> Mode_list;
    static Mode_list modes;

    Mode(char const *name, Filter_func *filter)
    : name(name), filter(filter)
    {
      // make sure that non-filtered mode is first in the list so that we
      // get this one displayed initially
      if (!filter)
        modes.push_front(this);
      else
        {
          Mode_list::Iterator i = modes.begin();
          if (i != modes.end())
            ++i;
          modes.insert_before(this, i);
        }
    }
  };

  void *get_head() const
  { return Kobject::from_dbg(Kobject_dbg::begin()); }

private:
  Mode::Mode_list::Const_iterator _current_mode;
  Filter_func *_filter;
};

//--------------------------------------------------------------------------
IMPLEMENTATION:

#include <climits>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "entry_frame.h"
#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "kobject.h"
#include "keycodes.h"
#include "ram_quota.h"
#include "simpleio.h"
#include "space.h"
#include "static_init.h"

Jdb_kobject_list::Mode::Mode_list Jdb_kobject_list::Mode::modes;

class Jdb_kobject_id_hdl : public Jdb_kobject_handler
{
public:
  virtual bool show_kobject(Kobject_common *, int) { return false; }
  virtual ~Jdb_kobject_id_hdl() {}
};

PUBLIC
bool
Jdb_kobject_id_hdl::invoke(Kobject_common *o, Syscall_frame *f, Utcb *utcb)
{
  if (   utcb->values[0] != Op_global_id
      && utcb->values[0] != Op_kobj_to_id)
    return false;

  if (utcb->values[0] == Op_global_id)
    utcb->values[0] = o->dbg_info()->dbg_id();
  else
    utcb->values[0] = Kobject_dbg::pointer_to_id((void *)utcb->values[1]);
  f->tag(Kobject_iface::commit_result(0, 1));
  return true;
}


PRIVATE
void *
Jdb_kobject_list::get_first()
{
  Kobject_dbg::Iterator f = Kobject_dbg::begin();
  while (f != Kobject_dbg::end() && _filter && !_filter(Kobject::from_dbg(f)))
    ++f;
  return Kobject::from_dbg(f);
}

PUBLIC explicit
Jdb_kobject_list::Jdb_kobject_list(Filter_func *filt)
: Jdb_list(), _current_mode(Mode::modes.end()), _filter(filt)
{ set_start(get_first()); }

PUBLIC
Jdb_kobject_list::Jdb_kobject_list()
: Jdb_list(), _current_mode(Mode::modes.begin())
{
  if (_current_mode != Mode::modes.end())
    _filter = _current_mode->filter;

  set_start(get_first());
}

PUBLIC
void
Jdb_kobject_list::show_item(String_buffer *buffer, void *item) const
{
  if (!item)
    return;
  Jdb_kobject::obj_description(buffer, false, static_cast<Kobject*>(item)->dbg_info());
}

PUBLIC
bool
Jdb_kobject_list::enter_item(void *item) const
{
  Kobject *o = static_cast<Kobject*>(item);
  return Jdb_kobject::module()->handle_obj(o, 1);
}

PUBLIC
void *
Jdb_kobject_list::follow_link(void *item)
{
  Kobject *o = static_cast<Kobject*>(item);
  if (Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o))
    return h->follow_link(o);

  return item;
}

PUBLIC
bool
Jdb_kobject_list::handle_key(void *item, int keycode)
{
  Kobject *o = static_cast<Kobject*>(item);
  bool handled = false;
  for (Jdb_kobject::Handler_iter h = Jdb_kobject::module()->global_handlers.begin();
       h != Jdb_kobject::module()->global_handlers.end(); ++h)
    handled |= h->handle_key(o, keycode);

  if (Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o))
    handled |= h->handle_key(o, keycode);

  return handled;
}

PRIVATE inline NOEXPORT
Kobject *
Jdb_kobject_list::next(Kobject *obj)
{
  if (!obj)
    return 0;

  Kobject_dbg::Iterator o = Kobject_dbg::Kobject_list::iter(obj->dbg_info());

  do
    {
      ++o;
      if (o == Kobject_dbg::end())
	return 0;
    }
  while (_filter && !_filter(Kobject::from_dbg(*o)));
  return Kobject::from_dbg(*o);
}

PRIVATE inline NOEXPORT
Kobject *
Jdb_kobject_list::prev(Kobject *obj)
{
  if (!obj)
    return 0;

  Kobject_dbg::Iterator o = Kobject_dbg::Kobject_list::iter(obj->dbg_info());

  do
    {
      --o;
      if (o == Kobject_dbg::end())
	return 0;
    }
  while (_filter && !_filter(Kobject::from_dbg(*o)));
  return Kobject::from_dbg(*o);
}

PUBLIC
int
Jdb_kobject_list::seek(int cnt, void **item)
{
  Kobject *c = static_cast<Kobject*>(*item);
  int i;
  if (cnt > 0)
    {
      for (i = 0; i < cnt; ++i)
	{
	  Kobject *n = next(c);
	  if (!n)
	    break;
	  c = n;
	}
    }
  else if (cnt < 0)
    {
      for (i = 0; i < -cnt; ++i)
	{
	  Kobject *n = prev(c);
	  if (!n)
	    break;
	  c = n;
	}
    }
  else
    return 0;

  if (*item != c)
    {
      *item = c;
      return i;
    }

  return 0;
}

PUBLIC
char const *
Jdb_kobject_list::show_head() const
{
  return "[Objects]";
}


PUBLIC
char const *
Jdb_kobject_list::get_mode_str() const
{
  if (_current_mode == Mode::modes.end())
    return "[Objects]";
  return _current_mode->name;
}



PUBLIC
void
Jdb_kobject_list::next_mode()
{
  if (_current_mode == Mode::modes.end())
    return;

  ++_current_mode;
  if (_current_mode == Mode::modes.end())
    _current_mode = Mode::modes.begin();

  _filter = _current_mode->filter;
}

/* When the mode changes the current object may get invisible,
 * get a new visible one */
PUBLIC
void *
Jdb_kobject_list::get_valid(void *o)
{
  if (!_filter)
    return o;

  if (_filter && _filter(static_cast<Kobject*>(o)))
    return o;
  return get_first();
}

IMPLEMENT
bool
Jdb_kobject_handler::invoke(Kobject_common *, Syscall_frame *, Utcb *)
{ return false; }

void *Jdb_kobject::kobjp;

IMPLEMENT
Jdb_kobject::Jdb_kobject()
  : Jdb_module("INFO")
{}


PUBLIC
void
Jdb_kobject::register_handler(Jdb_kobject_handler *h)
{
  if (h->is_global())
    global_handlers.push_back(h);
  else
    handlers.push_back(h);
}

PUBLIC
Jdb_kobject_handler *
Jdb_kobject::find_handler(Kobject_common *o)
{
  for (Handler_iter h = handlers.begin(); h != handlers.end(); ++h)
    {
      auto r = o->_cxx_dyn_type();
      if (r.type == h->kobj_type)
        return *h;

      // XXX: may be we should sort the handlers: most derived first
      cxx::uintptr_t delta;
      if (r.type->do_cast(h->kobj_type, cxx::Typeid<Kobject_common>::get(),
                          (cxx::uintptr_t)o - (cxx::uintptr_t)r.base, &delta))
        return *h;
    }

  return 0;
}

PUBLIC
bool
Jdb_kobject::handle_obj(Kobject *o, int lvl)
{
  if (Jdb_kobject_handler *h = find_handler(o))
    return h->show_kobject(o, lvl);

  return true;
}

PUBLIC static
char const *
Jdb_kobject::kobject_type(Kobject_common *o)
{
  if (Jdb_kobject_handler *h = module()->find_handler(o))
    return h->kobject_type(o);

  return Jdb_kobject_handler::_kobject_type(o);
}


PUBLIC static
void
Jdb_kobject::obj_description(String_buffer *buffer, bool dense, Kobject_dbg *o)
{
  buffer->printf(dense ? "%lx %lx [%-*s]" : "%8lx %08lx [%-*s]",
                 o->dbg_id(), (Mword)Kobject::from_dbg(o), 7, kobject_type(Kobject::from_dbg(o)));

  for (Handler_iter h = module()->global_handlers.begin();
       h != module()->global_handlers.end(); ++h)
    h->show_kobject_short(buffer, Kobject::from_dbg(o));

  if (Jdb_kobject_handler *oh = Jdb_kobject::module()->find_handler(Kobject::from_dbg(o)))
    oh->show_kobject_short(buffer, Kobject::from_dbg(o));
}

PRIVATE static
void
Jdb_kobject::print_kobj(Kobject *o)
{
  printf("%p [type=%s]", o, cxx::dyn_typeid(o)->name);
}

PUBLIC
Jdb_module::Action_code
Jdb_kobject::action(int cmd, void *&, char const *&, int &)
{
  if (cmd == 0)
    {
      puts("");
      Kobject_dbg::Iterator i = Kobject_dbg::pointer_to_obj(kobjp);
      if (i == Kobject_dbg::end())
	printf("Not a kobj.\n");
      else
        {
          Kobject *k = Kobject::from_dbg(i);
          if (!handle_obj(k, 0))
            printf("Kobj w/o handler: ");
          print_kobj(k);
          puts("");
        }
      return NOTHING;
    }
  else if (cmd == 1)
    {
      Jdb_kobject_list list;
      list.do_list();
    }
  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_kobject::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "K", "kobj", "%p", "K<kobj_ptr>\tshow information for kernel object", 
	  &kobjp },
	{ 1, "Q", "listkobj", "", "Q\tshow information for kernel objects", 0 },
    };
  return cs;
}

PUBLIC
int
Jdb_kobject::num_cmds() const
{ return 2; }

STATIC_INITIALIZE_P(Jdb_kobject, JDB_MODULE_INIT_PRIO);

PRIVATE static
int
Jdb_kobject::fmt_handler(char /*fmt*/, int *size, char const *cmd_str, void *arg)
{
  char buffer[20];

  int pos = 0;
  int c;
  Address n;

  *size = sizeof(void*);

  while((c = Jdb_core::cmd_getchar(cmd_str)) != ' ' && c != KEY_RETURN
        && c != KEY_RETURN_2)
    {
      if(c==KEY_ESC)
	return 3;

      if(c==KEY_BACKSPACE && pos>0)
	{
	  putstr("\b \b");
	  --pos;
	}

      if (pos < (int)sizeof(buffer) - 1)
	{
          Jdb_core::cmd_putchar(c);
	  buffer[pos++] = c;
	  buffer[pos] = 0;
	}
    }

  Kobject **a = (Kobject**)arg;

  if (!pos)
    {
      *a = 0;
      return 0;
    }

  char const *num = buffer;
  if (buffer[0] == 'P')
    num = buffer + 1;

  n = strtoul(num, 0, 16);

  Kobject_dbg::Iterator ko;

  if (buffer[0] != 'P')
    ko = Kobject_dbg::id_to_obj(n);
  else
    ko = Kobject_dbg::pointer_to_obj((void*)n);

  if (ko != Kobject_dbg::end())
    *a = Kobject::from_dbg(ko);
  else
    *a = 0;

  return 0;
}

PUBLIC static
void
Jdb_kobject::init()
{
  module();

  Jdb_core::add_fmt_handler('q', fmt_handler);

//  static Jdb_handler enter(at_jdb_enter);

  static Jdb_kobject_id_hdl id_hdl;
  module()->register_handler(&id_hdl);
}

PUBLIC static
Jdb_kobject *
Jdb_kobject::module()
{
  static Jdb_kobject jdb_kobj_module;
  return &jdb_kobj_module;
}

// Be robust if this object is invalid
PUBLIC static
void
Jdb_kobject::print_uid(Kobject_common *o, int task_format = 0)
{
  if (!o)
    {
      printf("%*.s", task_format, "---");
      return;
    }

  if (Kobject_dbg::is_kobj(o))
    {
      printf("%*.lx", task_format, o->dbg_info()->dbg_id());
      return;
    }

  printf("\033[31;1m%*s%p\033[m", task_format, "???", o);
  return;
}


extern "C" void
sys_invoke_debug(Kobject_iface *o, Syscall_frame *f)
{
  if (!o)
    {
      f->tag(Kobject_iface::commit_result(-L4_err::EInval));
      return;
    }

  Utcb *utcb = current_thread()->utcb().access();
  //printf("sys_invoke_debug: [%p] -> %p\n", o, f);
  Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o);
  if (h && h->invoke(o, f, utcb))
    return;

  for (Jdb_kobject::Handler_iter i = Jdb_kobject::module()->global_handlers.begin();
       i != Jdb_kobject::module()->global_handlers.end(); ++i)
    if (i->invoke(o, f, utcb))
      return;

  f->tag(Kobject_iface::commit_result(-L4_err::ENosys));
}

PUBLIC
template< typename T >
static
T *
Jdb_kobject_extension::find_extension(Kobject_common const *o)
{
  typedef Kobject_dbg::Dbg_ext_list::Iterator Iterator;
  for (Iterator ex = o->dbg_info()->_jdb_data.begin();
       ex != o->dbg_info()->_jdb_data.end(); ++ex)
    {
      if (!*ex)
        return 0;

      Jdb_kobject_extension *je = static_cast<Jdb_kobject_extension*>(*ex);
      if (je->type() == T::static_type)
	return static_cast<T*>(je);
    }

  return 0;
}

static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) all("[ALL]", 0);


