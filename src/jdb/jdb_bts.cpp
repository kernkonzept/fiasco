IMPLEMENTATION:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "initcalls.h"
#include "jdb.h"
#include "jdb_input.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_symbol.h"
#include "keycodes.h"
#include "kernel_console.h"
#include "mem_layout.h"
#include "panic.h"
#include "regdefs.h"
#include "static_init.h"
#include "vmem_alloc.h"

class Bts_entry;

class Jdb_bts : public Jdb_module
{
public:
  Jdb_bts() FIASCO_INIT;
private:
  static char  first_char;
  static Mword _absy;
  static Mword _addy;
};

enum
{
  Bts_entries = 44000,
  Bts_ctrl   = Mem_layout::Jdb_bts_area,
  Bts_start  = Mem_layout::Jdb_bts_area + 0x80,
  Bts_size   = Bts_entries * 12,
};

class Bts_entry
{
public:
  Unsigned32 from;
  Unsigned32 to;
  Unsigned32 predicted;
};

char  Jdb_bts::first_char;
Mword Jdb_bts::_absy  = 0;
Mword Jdb_bts::_addy  = 0;

PUBLIC inline
Space *
Bts_entry::task()
{
  return 0; //(predicted & 0xffff0000) >> 16;
}

PUBLIC inline
void
Bts_entry::task(Space * /*nr*/)
{
  //predicted = (predicted & 0x0000ffff) | (nr << 16);
}

PUBLIC inline
int
Bts_entry::pred()
{
  return (predicted & 0x10);
}

IMPLEMENT
Jdb_bts::Jdb_bts()
  : Jdb_module("MONITORING")
{
  if (Cpu::boot_cpu()->bts_type() != Cpu::Bts_unsupported)
    allocate();
}

FIASCO_INIT
void
Jdb_bts::allocate()
{
  for (Address addr=Bts_ctrl; addr<Bts_start+Bts_size; addr+=Config::PAGE_SIZE)
    {
      if (! Vmem_alloc::page_alloc((void*) addr, 
	    Vmem_alloc::ZERO_FILL, Vmem_alloc::User))
	panic("jdb_bts:: alloc buffer at "L4_PTR_FMT" failed", addr);
    }

  Unsigned32 *dsm = (Unsigned32*)Bts_ctrl;
  dsm[0] = Bts_start;
  dsm[1] = Bts_start;
  dsm[2] = Bts_start + Bts_size;
  dsm[3] = Bts_start + Bts_size;
  dsm[4] = Bts_start + Bts_size;
  dsm[5] = Bts_start + Bts_size;
  dsm[6] = Bts_start + Bts_size;
  dsm[7] = Bts_start + Bts_size;
  dsm[8] = 0;
  dsm[9] = 0;
  Cpu::wrmsr(Bts_ctrl, 0x600);
}

Bts_entry*
Jdb_bts::lookup(Mword idx)
{
  if (idx >= Bts_entries)
    return 0;

  Unsigned32 *dsm = (Unsigned32*)Bts_ctrl;
  Unsigned32 curr = (dsm[1] - Bts_start) / 12 - 1;

  if (curr >= Bts_entries)
    curr = Bts_entries-1;
  if (idx > curr)
    idx = Bts_entries - idx + curr;
  else
    idx = curr - idx;

  return (Bts_entry*)(Bts_start + 12*idx);
}

Mword
Jdb_bts::search_next_user_kernel(Mword idx)
{
  Bts_entry *e = lookup(idx), *e_end = lookup(Bts_entries-1);

  if (!e)
    return idx;

  bool in_kernel = Mem_layout::in_kernel(e->from);

  while (in_kernel == Mem_layout::in_kernel(e->from))
    {
      idx++;
      e--;
      if (e < (Bts_entry*)Bts_start)
	e = (Bts_entry*)(Bts_start + Bts_size - 12);
      if (e == e_end)
	break;
    }

  return idx;
}

void
Jdb_bts::set_task(Mword idx, Space *task)
{
  Bts_entry *e = lookup(idx), *e_end = lookup(Bts_entries-1);

  if (!e)
    return;

  while (!Mem_layout::in_kernel(e->from))
    {
      e->task(task);
      e--;
      if (e < (Bts_entry*)Bts_start)
	e = (Bts_entry*)(Bts_start + Bts_size - 12);
      if (e == e_end)
	break;
    }
}

bool
Jdb_bts::show_entry(Mword idx)
{
  Bts_entry *e;

  Kconsole::console()->getchar_chance();

  if (!(e = lookup(idx)))
    return false;

  printf(" %5ld: %08x => %08x %c", 
         idx, e->from, e->to, e->pred() ? 'p' : ' ');

  Space *task = e->task();

  if (Mem_layout::in_kernel(e->from) || task != 0)
    {
      Address sym_addr = e->from;
      char buffer[64];

      if (Jdb_symbol::match_addr_to_symbol_fuzzy(&sym_addr, task,
						 buffer,
						 46 < sizeof(buffer)
						   ? 46 : sizeof(buffer))
	  && e->from-sym_addr < 1024)
	{
	  printf("   %s", buffer);
	  if (e->from-sym_addr)
	    printf(" %s+ 0x%lx\033[m", Jdb::esc_line, e->from-sym_addr);
	}
    }
  puts("\033[K");
  
  return true;
}

void
Jdb_bts::show()
{
  Mword lines = Jdb_screen::height()-1;
  bool   status_redraw = true;

  Jdb::clear_screen();

  for (;;)
    {
      Mword i;
      int c;

      Jdb::cursor(1, 1);
      for (i=0; i<lines; i++)
	if (!show_entry(_absy + i))
	  puts("\033[K");

      for (bool redraw=false; !redraw;)
	{
	  if (status_redraw)
	    {
	      Jdb::printf_statline("bts", 
				   "<Tab>=next usr/krnl <Space>=set task",
				   "_");
	      status_redraw = false;
	    }
	  Jdb::cursor(_addy+1, 1);
	  putstr(Jdb::esc_emph);
	  show_entry(_absy+_addy);
	  putstr("\033[m");
	  Jdb::cursor(_addy+1, 1);
	  c = Jdb_core::getchar();
	  show_entry(_absy+_addy);

	  if (Jdb::std_cursor_key(c, 0, lines, Bts_entries - lines,
				  &_absy, &_addy, 0, &redraw))
	    continue;

	  switch (c)
	    {
	    case KEY_ESC:
	      return;
	    case ' ':
	      Jdb::printf_statline("bts", "<CR>=commit change", "task=");
	      Jdb::cursor(Jdb_screen::height(), 11);
	      Mword new_task;
	      if (Jdb_input::get_mword(&new_task, 8, 16))
		{
		  set_task(_absy+_addy, (Space*)new_task);
		  redraw = true;
		}
	      break;
	    case KEY_RETURN:
	    case KEY_RETURN_2:
	      if (Jdb_disasm::avail())
		{
		  Bts_entry *e = Jdb_bts::lookup(_absy+_addy);
		  if (e)
		    {
		      if (!Jdb_disasm::show(e->from, e->task(), 1))
			return;
		      redraw = true;
		      status_redraw = true;
		    }
		}
	      break;
	    case KEY_TAB:
	      _absy = search_next_user_kernel(_absy);
	      if (_absy > Bts_entries - lines)
		_absy = Bts_entries - lines;
	      _addy = 0;
	      redraw = true;
	      break;
	    default:
	      if (Jdb::is_toplevel_cmd(c)) 
		return;
	      break;
	    }
	}
    }
}

PUBLIC
Jdb_module::Action_code
Jdb_bts::action(int, void *&, char const *&, int &)
{
  if (Cpu::boot_cpu()->bts_type() == Cpu::Bts_unsupported)
    {
      printf("Branch trace store unsupported\n");
      return NOTHING;
    }

  switch (first_char)
    {
    case '+':
    case '-':
      Cpu::boot_cpu()->bts_enable(first_char == '+');
      putchar(first_char);
      putchar('\n');
      break;

    default:
      show();
      break;
    }

  return NOTHING;
}

PUBLIC
int
Jdb_bts::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const * Jdb_bts::cmds() const
{
  static Cmd cs[] = 
    {
	{ 0, "Z", "bts", "%C",
	  "Z[{+|-}]\tshow BTS records, enable/disable BTS recording",
	  &first_char },
    };
  return cs;
}

static Jdb_bts jdb_bts INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
