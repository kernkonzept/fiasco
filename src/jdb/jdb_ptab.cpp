IMPLEMENTATION:

#include <cstdio>

#include "config.h"
#include "jdb.h"
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
  bool show_kobject(Kobject_common *, int) { return false; }
};

class Jdb_ptab : public Jdb_table
{
private:
  Address base;
  Address virt_base;
  int _level;
  Space *_task;
  unsigned entries;
  unsigned char cur_pt_level;
  char dump_raw;

  static unsigned entry_is_pt_ptr(Pdir::Pte_ptr const &entry,
                                  unsigned *entries, unsigned *next_level);
  static Address entry_phys(Pdir::Pte_ptr const &entry);
  static void *entry_virt(Pdir::Pte_ptr const &entry);

  void print_entry(Pdir::Pte_ptr const &);
  void print_head(void *entry);
};

char Jdb_ptab_m::first_char;

PUBLIC
Jdb_ptab::Jdb_ptab(void *pt_base = 0, Space *task = 0,
                   unsigned char pt_level = 0, unsigned entries = 0,
                   Address virt_base = 0, int level = 0)
: base((Address)pt_base), virt_base(virt_base), _level(level),
  _task(task), entries(entries), cur_pt_level(pt_level), dump_raw(0)
{
  if (entries == 0)
    this->entries = Pdir::Levels::length(pt_level);
}

PUBLIC
unsigned
Jdb_ptab::col_width(unsigned column) const
{
  if (column == 0)
    return Jdb_screen::Col_head_size;
  else
    return sizeof(Pdir::Pte_ptr::Entry) * 2;
}

PUBLIC
unsigned long
Jdb_ptab::cols() const
{
  return Jdb_screen::cols(sizeof(Mword) * 2, sizeof(Pdir::Pte_ptr::Entry) * 2);
}


// available from the jdb_dump module
int jdb_dump_addr_task(Address addr, Space *task, int level)
  __attribute__((weak));


PUBLIC
void
Jdb_ptab::draw_entry(unsigned long row, unsigned long col)
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
    print_entry(Pdir::Pte_ptr(pte(idx), cur_pt_level));
  else
    putstr("   ###  ");
}

IMPLEMENT
Address
Jdb_ptab::entry_phys(Pdir::Pte_ptr const &entry)
{
  if (!entry.is_leaf())
    return entry.next_level();

  return entry.page_addr();
}

IMPLEMENT_DEFAULT
void *
Jdb_ptab::entry_virt(Pdir::Pte_ptr const &entry)
{
  return (void*)Mem_layout::phys_to_pmem(entry_phys(entry));
}


PRIVATE inline
int
Jdb_ptab::index(unsigned row, unsigned col)
{
  Mword e = (col-1) + (row * (cols()-1));
  if (e < Pdir::Levels::length(cur_pt_level))
    return e;
  else
    return -1;
}


PRIVATE inline
void *
Jdb_ptab::pte(int index)
{
  return (void*)(base + index * Pdir::Levels::entry_size(cur_pt_level));
}

IMPLEMENT
unsigned
Jdb_ptab::entry_is_pt_ptr(Pdir::Pte_ptr const &entry,
                          unsigned *entries, unsigned *next_level)
{
  if (!entry.is_valid() || entry.is_leaf())
    return 0;

  unsigned n = 1;
  while (   (entry.level + n) < Pdir::Depth
         && Pdir::Levels::length(entry.level + n) <= 1)
    ++n;

  *entries = Pdir::Levels::length(entry.level + n);
  *next_level = entry.level + n;
  return 1;
}


IMPLEMENT
void
Jdb_ptab::print_head(void *entry)
{
  printf(L4_PTR_FMT, (Address)entry);
}

PUBLIC
bool
Jdb_ptab_m::handle_key(Kobject_common *o, int code)
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

  Jdb_ptab pt_view(static_cast<Mem_space*>(t)->dir(), t, 0, 0, 0, 1);
  pt_view.show(0,0);

  return true;
}

PUBLIC
unsigned 
Jdb_ptab::key_pressed(int c, unsigned long &row, unsigned long &col)
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
      dump_raw ^= 1;
      return Redraw;

    case KEY_RETURN:	// goto ptab/address under cursor
    case KEY_RETURN_2:
      if (_level<=7)
	{
          int idx = index(row, col);
          if (idx < 0)
            break;

          Pdir::Pte_ptr pt_entry(pte(idx), cur_pt_level);
	  if (!pt_entry.is_valid())
	    break;

	  void *pd_virt = entry_virt(pt_entry);

	  unsigned next_level, entries;

	  if (cur_pt_level < Pdir::Depth
	      && entry_is_pt_ptr(pt_entry, &entries, &next_level))
	    {
	      Jdb_ptab pt_view(pd_virt, _task, next_level, entries,
		               disp_virt(idx), _level+1);
	      if (!pt_view.show(0,1))
		return Exit;
	      return Redraw;
	    }
	  else if (jdb_dump_addr_task != 0)
	    {
	      if (!jdb_dump_addr_task(disp_virt(idx), _task, _level+1))
		return Exit;
	      return Redraw;
	    }
	}
      break;
    }

  return Handled;
}

PRIVATE
Address
Jdb_ptab::disp_virt(int idx)
{
  Pdir::Va e((Mword)idx << Pdir::lsb_for_level(cur_pt_level));
  return Virt_addr::val(e) + virt_base;
}

PUBLIC
unsigned long
Jdb_ptab::rows() const
{
  if (cols() > 1)
    return (entries + cols() - 2) / (cols()-1);
  return 0;
}

PUBLIC
void
Jdb_ptab::print_statline(unsigned long row, unsigned long col)
{
  unsigned long sid = Kobject_dbg::pointer_to_id(_task);

  Address va;
  int idx = index(row, col);
  if (idx >= 0)
    va = disp_virt(idx);
  else
    va = -1;

  Jdb::printf_statline("p:", "<Space>=mode <CR>=goto page/next level",
                       "<level=%1d> <" L4_PTR_FMT "> task D:%lx", cur_pt_level, va, sid);
}

PUBLIC
Jdb_module::Action_code
Jdb_ptab_m::action(int cmd, void *&args, char const *&fmt, int &next_char)
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
	    {
	      task = 0; //Jdb::get_current_task();
	    }
	}
      else if (args == &task)
	{
#if 0
	  if (!Jdb::is_valid_task(task))
	    {
	      puts(" invalid task");
	      return NOTHING;
	    }
#endif
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
        s = Kernel_task::kernel_task();

      void *ptab_base;
      if (!(ptab_base = ((void*)static_cast<Mem_space*>(s)->dir())))
	return Jdb_module::NOTHING;

      Jdb::clear_screen();
      Jdb_ptab pt_view(ptab_base, s);
      pt_view.show(0,1);

    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_ptab_m::cmds() const
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
Jdb_ptab_m::num_cmds() const
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
