IMPLEMENTATION:

#include <cstdio>

#include "config.h"
#include "jdb.h"
#include "jdb_disasm.h"
#include "jdb_kobject.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_table.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "kmem.h"
#include "keycodes.h"
#include "space.h"
#include "task.h"
#include "thread_object.h"
#include "static_init.h"
#include "types.h"

class Jdb_ptab_m : public Jdb_module, public Jdb_kobject_handler
{
public:
  Jdb_ptab_m() FIASCO_INIT;

private:
  Address task;
  static char first_char;
  bool show_kobject(Kobject_common *, int) override
  { return false; }
  void show_ptab(void *ptab_base, Space *s, int level = 0);
};

class Jdb_ptab_base : public Jdb_table
{
private:
  virtual bool entry_is_valid(void *pte) const = 0;
  virtual bool entry_is_pt_ptr(void *pte, unsigned *next_pt_level) const = 0;
  virtual bool show_next_level(int idx, unsigned next_level) = 0;
  virtual unsigned entry_col_width() const = 0;
  virtual unsigned long pdir_levels_length(unsigned level) const = 0;
  virtual unsigned long pdir_levels_entry_size(unsigned level) const = 0;
  virtual void print_entry(void *pte) const = 0;
  virtual Address disp_virt(int idx) const = 0;

protected:
  Address _base;
  Address _virt_base;
  int _level;
  Space *_task;
  unsigned char _pt_level;
  char _dump_raw;
};

char Jdb_ptab_m::first_char;

// available from the jdb_dump module
int jdb_dump_addr_task(Jdb_address addr, int level)
  __attribute__((weak));

PUBLIC
Jdb_ptab_base::Jdb_ptab_base(void *pt_base = 0, Space *task = 0,
                             unsigned char pt_level = 0,
                             Address virt_base = 0, int level = 0)
: _base(reinterpret_cast<Address>(pt_base)), _virt_base(virt_base),
  _level(level), _task(task), _pt_level(pt_level), _dump_raw(0)
{
}

PUBLIC
unsigned
Jdb_ptab_base::key_pressed(int c, unsigned long &row, unsigned long &col) override
{
  switch (c)
    {
    default:
      return Nothing;

    case KEY_CURSOR_HOME: // return to previous or go home
      if (_level == 0)
	return Nothing;
      return Back;

    case ' ':
      _dump_raw ^= 1;
      return Redraw;

    case 'u':           // disassemble using address the cursor points to
    case KEY_RETURN:    // goto ptab/address under cursor
    case KEY_RETURN_2:
      if (_level <= 7)
        {
          int idx = index(row, col);
          if (idx < 0)
            break;

          void *e = pte(idx);
          if (!entry_is_valid(e))
            break;

          unsigned next_pt_level;
          bool is_pt_ptr = entry_is_pt_ptr(e, &next_pt_level);
          Jdb_address virt(is_pt_ptr ? 0 : disp_virt(idx), _task);

          if (c == 'u')
            {
              if (Jdb_disasm::avail() && !is_pt_ptr)
                {
                  char s[16];
                  Jdb::printf_statline("p", "<CR>=disassemble here",
                                       "u[address=" L4_PTR_FMT " %s] ",
                                       virt.addr(),
                                       Jdb::addr_space_to_str(virt,
                                                              s, sizeof(s)));
                  int c1 = Jdb_core::getchar();
                  if (c1 != KEY_RETURN && c1 != ' ' && c != KEY_RETURN_2)
                    {
                      Jdb::printf_statline("p", 0, "u");
                      Jdb::execute_command("u", c1);
                      return Exit;
                    }

                  return Jdb_disasm::show(virt, _level+1)
                    ? Redraw
                    : Exit;
                }
            }
          else
            {
              if (is_pt_ptr)
                return show_next_level(idx, next_pt_level) ? Redraw : Exit;
              else if (jdb_dump_addr_task != 0)
                return jdb_dump_addr_task(virt, _level + 1) ? Redraw : Exit;
            }
        }
      break;
    }

  return Handled;
}

PUBLIC
unsigned
Jdb_ptab_base::col_width(unsigned column) const override
{
  if (column == 0)
    return Jdb_screen::Col_head_size;
  else
    return entry_col_width();
}

PUBLIC
unsigned long
Jdb_ptab_base::cols() const override
{
  return Jdb_screen::cols(sizeof(Mword) * 2, entry_col_width());
}

PUBLIC
void
Jdb_ptab_base::draw_entry(unsigned long row, unsigned long col) override
{
  int idx;
  if (col == 0)
    {
      idx = index(row, 1);
      if (idx >= 0)
        print_head(pte(idx));
      else
        putstr("        ");
    }
  else if ((idx = index(row, col)) >= 0)
    print_entry(pte(idx));
  else
    print_invalid();
}

PRIVATE inline
int
Jdb_ptab_base::index(unsigned row, unsigned col)
{
  Mword e = (col - 1) + (row * (cols() - 1));
  if (e < pdir_levels_length(_pt_level))
    return e;
  else
    return -1;
}

PROTECTED inline
void *
Jdb_ptab_base::pte(int index) const
{
  return reinterpret_cast<void *>(
           _base + index * pdir_levels_entry_size(_pt_level));
}

PRIVATE
void
Jdb_ptab_base::print_head(void *entry) const
{
  printf(L4_PTR_FMT, reinterpret_cast<Address>(entry));
}

PUBLIC
unsigned long
Jdb_ptab_base::rows() const override
{
  if (cols() <= 1)
    return 0;

  return (pdir_levels_length(_pt_level) + cols() - 2) / (cols() - 1);
}

PUBLIC
void
Jdb_ptab_base::print_statline(unsigned long row, unsigned long col) override
{
  unsigned long sid = Kobject_dbg::pointer_to_id(_task);

  Address va;
  int idx = index(row, col);
  if (idx >= 0)
    va = disp_virt(idx);
  else
    va = -1;

  Jdb::printf_statline("p:", "<Space>=mode <CR>=goto page/next level",
                       "<level=%1d> <" L4_PTR_FMT "> task D:%lx", _pt_level, va, sid);
}


// -----------------------------------------------------------------------
template< typename T >
class Jdb_ptab_pdir : public Jdb_ptab_base
{
  typedef typename T::Pte_ptr T_pte_ptr;
  using Jdb_ptab_base::Jdb_ptab_base;
private:
  void *entry_virt(T_pte_ptr const &entry) const;
};

PRIVATE template< typename T >
bool
Jdb_ptab_pdir<T>::entry_is_valid(void *pte) const override
{
  return T_pte_ptr(pte, _pt_level).is_valid();
}

PRIVATE template< typename T >
bool
Jdb_ptab_pdir<T>::entry_is_pt_ptr(void *pte, unsigned *next_pt_level) const override
{
  T_pte_ptr entry(pte, _pt_level);

  if (!entry.is_valid() || entry.is_leaf())
    return false;

  unsigned n = 1;
  while (   (entry.level + n) < T::Depth
         && pdir_levels_length(entry.level + n) <= 1)
    ++n;

  *next_pt_level = entry.level + n;
  return true;
}

IMPLEMENT_DEFAULT template< typename T >
void *
Jdb_ptab_pdir<T>::entry_virt(T_pte_ptr const &entry) const
{
  Address phys = entry_phys(entry);
  return reinterpret_cast<void*>(Mem_layout::phys_to_pmem(phys));
}

PRIVATE template< typename T >
Address
Jdb_ptab_pdir<T>::entry_phys(T_pte_ptr const &entry) const
{
  if (!entry.is_leaf())
    return entry.next_level();

  return entry.page_addr();
}

PRIVATE template< typename T >
unsigned
Jdb_ptab_pdir<T>::entry_col_width() const override
{
  return sizeof(typename Jdb_ptab_pdir<T>::T_pte_ptr::Entry) * 2;
}

PRIVATE template< typename T >
bool
Jdb_ptab_pdir<T>::show_next_level(int idx, unsigned next_pt_level) override
{
  void *e = pte(idx);
  T_pte_ptr pt_entry(e, _pt_level);
  Jdb_ptab_pdir<T> pt_view(entry_virt(T_pte_ptr(e, _pt_level)), _task,
                           next_pt_level, disp_virt(idx), _level + 1);
  return pt_view.show(0, 1);
}

PRIVATE template< typename T >
unsigned long
Jdb_ptab_pdir<T>::pdir_levels_length(unsigned level) const override
{
  return T::Levels::length(level);
}

PRIVATE template< typename T >
unsigned long
Jdb_ptab_pdir<T>::pdir_levels_entry_size(unsigned level) const override
{
  return T::Levels::entry_size(level);
}

PRIVATE template< typename T >
Address
Jdb_ptab_pdir<T>::disp_virt(int idx) const override
{
  typename T::Va e(static_cast<Mword>(idx) << T::lsb_for_level(_pt_level));
  return cxx::int_value<Virt_addr>(e) + _virt_base;
}

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry(void *pte) const override
{
  return print_entry(T_pte_ptr(pte, _pt_level));
}

// -----------------------------------------------------------------------
PUBLIC
bool
Jdb_ptab_m::handle_key(Kobject_common *o, int code) override
{
  if (code != 'p')
    return false;

  Space *t = cxx::dyn_cast<Task*>(o);
  if (!t)
    {
      Thread *th = cxx::dyn_cast<Thread*>(o);
      if (!th || !th->space())
        return false;

      t = th->space();
    }

  show_ptab(static_cast<Mem_space*>(t)->dir(), t, 1);

  return true;
}

PUBLIC
char const *
Jdb_ptab_m::help_text(Kobject_common *o) const override
{
  Thread *t;
  if (cxx::dyn_cast<Task*>(o) || ((t = cxx::dyn_cast<Thread*>(o)) && t->space()))
    return "p=ptab";

  return 0;
}

PUBLIC
Jdb_module::Action_code
Jdb_ptab_m::action(int cmd, void *&args, char const *&fmt,
                   int &next_char) override
{
  if (cmd == 0)
    {
      if (args == &first_char)
	{
	  if (first_char != KEY_RETURN && first_char != KEY_RETURN_2
              && first_char != ' ')
	    {
	      fmt       = "%q";
	      args      = &task;
	      next_char = first_char;
	      return EXTRA_INPUT_WITH_NEXTCHAR;
	    }
	  else
            task = 0; // use current task -- see below
	}
      else if (args == &task)
	{
	}
      else
	return NOTHING;

      Space *s;
      if (task)
        {
          s = cxx::dyn_cast<Task*>(reinterpret_cast<Kobject*>(task));
          if (!s)
	    return Jdb_module::NOTHING;
        }
      else
        s = Jdb::get_space(Jdb::triggered_on_cpu);

      void *ptab_base;
      if (!(ptab_base = static_cast<Mem_space*>(s)->dir()))
	return Jdb_module::NOTHING;

      Jdb::clear_screen();
      show_ptab(ptab_base, s);
    }

  return NOTHING;
}

IMPLEMENT_DEFAULT
void
Jdb_ptab_m::show_ptab(void *ptab_base, Space *s, int level = 0)
{
  Jdb_ptab_pdir<Pdir> pt_view(ptab_base, s, 0, 0, level);
  pt_view.show(0, 1);
}

PUBLIC
Jdb_module::Cmd const *
Jdb_ptab_m::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "p", "ptab", "%C",
	  "p[<taskno>]\tshow pagetable of current/given task",
	  &first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_ptab_m::num_cmds() const override
{
  return 1;
}

IMPLEMENT
Jdb_ptab_m::Jdb_ptab_m()
  : Jdb_module("INFO")
{
  Jdb_kobject::module()->register_handler(this);
}

static Jdb_ptab_m jdb_ptab_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE
void
Jdb_ptab_base::print_invalid() const
{
  if constexpr (sizeof(unsigned long) == 4)
    putstr("   ###  ");
  else
    putstr("       ###      ");
}
