IMPLEMENTATION:

#include <cstdio>

#include "config.h"
#include "jdb.h"
#include "jdb_screen.h"
#include "jdb_table.h"
#include "jdb_kobject.h"
#include "kernel_console.h"
#include "kmem.h"
#include "keycodes.h"
#include "space.h"
#include "task.h"
#include "thread_object.h"
#include "static_init.h"
#include "types.h"

class Jdb_obj_space : public Jdb_table, public Jdb_kobject_handler
{
public:
  enum Mode
  {
    Name,
    Raw,
    End_mode
  };

private:
  Address _base;
  Space  *_task;
  Mode    _mode;

  bool show_kobject(Kobject_common *, int) override
  {
    return false;
  }
};

static inline
Jdb_obj_space::Mode
operator ++ (Jdb_obj_space::Mode &m)
{
  long _m = m;
  ++_m;
  if (_m >= Jdb_obj_space::End_mode)
    _m = 0;

  m = Jdb_obj_space::Mode(_m);

  return m;
}

PUBLIC
Jdb_obj_space::Jdb_obj_space(Address base = 0, int level = 0)
: Jdb_table(1),
  _base(base),
  _task(nullptr),
  _mode(Name)
{
  (void)level;
  Jdb_kobject::module()->register_handler(this);
}

PUBLIC
unsigned
Jdb_obj_space::col_width(unsigned column) const override
{ return (column == 0) ? 6 : 16; }

PUBLIC
unsigned long
Jdb_obj_space::cols() const override
{ return Jdb_screen::cols(col_width(0), col_width(1)); }

PUBLIC
unsigned long
Jdb_obj_space::rows() const override
{ return Obj_space::Map_max_address / (cols() - 1); }

PUBLIC
void
Jdb_obj_space::print_statline(unsigned long row, unsigned long col) override
{
  static String_buf<128> buf;
  static String_buf<80> help;
  buf.clear();
  help.clear();
  unsigned rights;

  Kobject_iface *o = item(index(row,col), &rights);
  if (!o)
    {
      Jdb::cursor(Jdb_screen::height() - 1, 1);
      Jdb::clear_to_eol();
      Jdb::printf_statline("objs", "<Space>=mode", "%lx: -- INVALID --",
                           cxx::int_value<Cap_index>(index(row,col)));
      return;
    }

  Jdb_kobject::obj_description(&buf, &help, true, o->dbg_info());
  Jdb::cursor(Jdb_screen::height() - 1, 1);
  printf("%*s", Jdb_screen::width(), help.c_str());
  Jdb::printf_statline("objs", "<Space>=mode",
                       "%lx_r%02x: %.*s",
                       cxx::int_value<Cap_index>(index(row, col)),
                       rights, buf.length(), buf.begin());
}

PUBLIC
void
Jdb_obj_space::print_entry(Cap_index entry)
{
  unsigned rights;
  Kobject_iface *o = item(entry, &rights);

  if (!o)
    printf("       --       ");
  else
    {
      switch (_mode)
        {
        case Name:
          {
            // ANSI escape sequences in the object type string must be taken
            // into account when specifying the padding width for that string.
            char const *ot = Jdb_kobject::kobject_type(o);
            int invisible_chars = Jdb::invisible_len(ot);
            printf("%05lx%c%c%c %-*s",
                   o->dbg_info()->dbg_id(),
                   rights & 8 ? 'D' : '-',
                   rights & 2 ? 'S' : '-',
                   rights & 1 ? 'W' : '-',
                   7 + invisible_chars, ot);
            break;
          }
        case Raw:
        default:
          printf("%16lx", reinterpret_cast<Mword>(o) | rights);
          break;
        }
    }
}

PUBLIC
void
Jdb_obj_space::draw_entry(unsigned long row, unsigned long col) override
{
  if (col == 0)
    printf("%06lx", cxx::int_value<Cap_index>(index(row, 1)));
  else
    print_entry(index(row, col));
}

PRIVATE
Cap_index
Jdb_obj_space::index(unsigned long row, unsigned long col)
{
  Mword e = (col - 1) + (row * (cols() - 1));
  return Cap_index(_base + e);
}

PRIVATE
bool
Jdb_obj_space::handle_user_keys(int c, Kobject_iface *o)
{
  if (!o)
    return false;

  // in case of overlayprint
  Jdb::cursor(3, 1);

  bool handled = false;
  for (auto const &h : Jdb_kobject::module()->global_handlers)
    handled |= h->handle_key(o, c);

  if (Jdb_kobject_handler *h = Jdb_kobject::module()->find_handler(o))
    handled |= h->handle_key(o, c);

  return handled;
}

PUBLIC
unsigned
Jdb_obj_space::key_pressed(int c, unsigned long &row,
                           unsigned long &col) override
{
  switch (c)
    {
    default:
      {
        unsigned rights;
        if (handle_user_keys(c, item(index(row, col), &rights)))
          return Redraw;
        return Nothing;
      }

    case KEY_CURSOR_HOME: // return to previous or go home
      return Back;

    case ' ':
      ++_mode;
      return Redraw;
    }
}

PUBLIC
bool
Jdb_obj_space::handle_key(Kobject_common *o, int code) override
{
  if (code != 'o')
    return false;

  Space *t = cxx::dyn_cast<Task*>(o);
  if (!t)
    {
      Thread *th = cxx::dyn_cast<Thread *>(o);
      if (!th || !th->space())
        return false;

      t = th->space();
    }

  _task = t;
  show(0, 0);

  return true;
}

PUBLIC
char const *
Jdb_obj_space::help_text(Kobject_common *o) const override
{
  Thread *t;
  if (cxx::dyn_cast<Task*>(o) || ((t = cxx::dyn_cast<Thread *>(o)) && t->space()))
    return "o=objspace";

  return nullptr;
}

static Jdb_obj_space jdb_obj_space INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PUBLIC
Kobject_iface *
Jdb_obj_space::item(Cap_index entry, unsigned *rights)
{
  Obj_space::Entry *c = _task->jdb_lookup_cap(entry);

  if (!c)
    return nullptr;

  Kobject_iface *o = c->obj();
  *rights = cxx::int_value<Obj::Attr>(c->rights());

  return o;
}
