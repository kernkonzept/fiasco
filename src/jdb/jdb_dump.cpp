INTERFACE:

#include "types.h"
#include "l4_types.h"

class Space;


IMPLEMENTATION:

#include <cstdio>
#include <cctype>

#include "config.h"
#include "jdb.h"
#include "jdb_disasm.h"
#include "jdb_table.h"
#include "jdb_input.h"
#include "jdb_input_task.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_symbol.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "simpleio.h"
#include "static_init.h"
#include "types.h"
#include "task.h"

class Jdb_dump : public Jdb_module, public Jdb_table
{
public:
  Jdb_dump() FIASCO_INIT;
  unsigned long cols() const { return Jdb_screen::cols(); }
  unsigned long rows() const { return Jdb_screen::rows(); }
  void draw_entry(unsigned long row, unsigned long col);
  void print_statline();

private:
  enum
    {
      B_MODE = 'b',	// byte
      C_MODE = 'c',	// char
      D_MODE = 'd',	// word
    };
  
  static const unsigned elb = sizeof(Mword);
  static char show_adapter_memory;
  static Address highlight_start, highlight_end;

  int level;
  Space *task;
  char dump_type;
  static Address virt_addr;
};

char    Jdb_dump::show_adapter_memory;
Address Jdb_dump::highlight_start;
Address Jdb_dump::highlight_end;
Address Jdb_dump::virt_addr;

// if we have the APIC module
extern int ignore_invalid_apic_reg_access
  __attribute__((weak));

PRIVATE inline
Address
Jdb_dump::virt(unsigned long row, unsigned long col)
{
  return (col-1) * elb + row * (cols()-1) * elb;
}

PUBLIC
unsigned
Jdb_dump::col_width(unsigned col) const
{
  if (col == 0)
    return Jdb_screen::Col_head_size;
  if (dump_type == C_MODE)
    return Jdb_screen::Mword_size_cmode;
  else
    return Jdb_screen::Mword_size_bmode; 
}

PUBLIC
void
Jdb_dump::print_statline(unsigned long row, unsigned long col)
{
  char const *s = dump_type == D_MODE
                  ? "e=edit u=disasm D=dump <Space>=mode <CR>=goto addr"
                  : "<Space>=mode";
  if (task)
    Jdb::printf_statline("dump", s, "%c<" L4_PTR_FMT "> task %p",
                         dump_type, virt(row, col), task);
  else
    Jdb::printf_statline("dump", s, "%c<" L4_PTR_FMT "> physical",
	                 dump_type, virt(row, col));
}

IMPLEMENT
void
Jdb_dump::draw_entry(unsigned long row, unsigned long col)
{
  if (col == 0)
    {
      printf("%0*lx", (int)col_width(col), row * (cols()-1) * elb);
      return;
    }

  Address entry = virt(row, col);

  // prevent apic from getting confused by invalid register accesses
  if (&ignore_invalid_apic_reg_access)
    ignore_invalid_apic_reg_access = 1;

  Mword dummy;
  bool mapped  = Jdb::peek((Mword*)entry, task, dummy);
  bool ram     = !Jdb::is_adapter_memory(entry, task);

  if (mapped)
    {
      if (ram || show_adapter_memory)
	{
	  Mword mword;
	  Jdb::peek((Mword*)entry, task, mword);

	  if (dump_type==D_MODE)
	    {
	      if (mword == 0)
		printf("%*lu", (int)Jdb_screen::Mword_size_bmode, mword);
	      else if (mword == (Mword)~0UL)
		printf("%*d", (int)Jdb_screen::Mword_size_bmode, -1);
	      else
	        {
		  if (highlight_start <= mword && mword <= highlight_end)
		    printf("%s" L4_PTR_FMT "\033[m", Jdb::esc_emph, mword);
		  else
		    printf(L4_PTR_FMT, mword);
		}
	    }
	  else if (dump_type==B_MODE)
	    {
	      for (Mword u=0; u<elb; u++)
                printf("%02lx", (mword >> (8 * u)) & 0xff);
	    }
	  else if (dump_type==C_MODE)
	    {
	      for (Mword u=0; u<elb; u++)
		{
		  Unsigned8 b = (mword >> (8*u)) & 0xff;
		  putchar(b>=32 && b<=126 ? b : '.');
		}
	    }
	}
      else // is_adapter_memory
	{
	  if (dump_type == C_MODE)
	    printf("%.*s", (int)Jdb_screen::Mword_size_cmode, Jdb_screen::Mword_adapter);
	  else
	    printf("%.*s", (int)Jdb_screen::Mword_size_bmode, Jdb_screen::Mword_adapter);
	}
    }
  else // !mapped
    {
      if (dump_type == C_MODE)
	printf("%.*s", (int)Jdb_screen::Mword_size_cmode, Jdb_screen::Mword_not_mapped);
      else
        printf("%.*s", (int)Jdb_screen::Mword_size_bmode, Jdb_screen::Mword_not_mapped);
    }
  
  if (&ignore_invalid_apic_reg_access)
    ignore_invalid_apic_reg_access = 0;
}

PUBLIC
Jdb_module::Action_code
Jdb_dump::dump(Address virt, Space *task, int level)
{ //printf("DU: %p %lx\n", task, virt); Jdb::getchar();
  int old_l = this->level;
  this->level = level;
  this->task = task;
  dump_type = D_MODE;
  if (!level)
    Jdb::clear_screen();

  unsigned long row =  virt / ((cols()-1) * elb);
  unsigned long col = (virt % ((cols()-1) * elb)) / elb + 1;
  bool r = show(row, col);
  this->level = old_l;
  return r ? EXTRA_INPUT : NOTHING;
}

PUBLIC
bool
Jdb_dump::edit_entry(unsigned long row, unsigned long col, unsigned cx, unsigned cy)
{
  Address entry = virt(row,col);

  Mword mword;
  if (!Jdb::peek((Mword*)entry, task, mword))
    return false;

  putstr(Jdb_screen::Mword_blank);
  Jdb::printf_statline("dump", "<CR>=commit change",
		       "edit <" L4_PTR_FMT "> = " L4_PTR_FMT, entry, mword);

  Jdb::cursor(cy, cx);
  Mword new_mword;
  if (Jdb_input::get_mword(&new_mword, sizeof(Mword)*2, 16))
    Jdb::poke((Mword*)entry, task, new_mword);

  return true; // redraw
}

PUBLIC
unsigned
Jdb_dump::key_pressed(int c, unsigned long &row, unsigned long &col)
{
  switch (c)
    {
    default:
      return Nothing;

    case KEY_CURSOR_HOME: // return to previous or go home
      if (level == 0)
	{
	  Address v = virt(row, col);
	  if (v == 0)
	    return Handled;

	  if ((v & ~Config::PAGE_MASK) == 0)
	    row -= Config::PAGE_SIZE / 32;
	  else
	    {
	      col = 1;
	      row = (v & Config::PAGE_MASK) / 32;
	    }
	  return Redraw;
	}
      return Back;

    case KEY_CURSOR_END:
	{
	  Address v = virt(row, col);
	  if ((v & ~Config::PAGE_MASK) >> 2 == 0x3ff)
	    row += Config::PAGE_SIZE / 32;
	  else
	    {
	      col = Jdb_screen::cols() - 1;
	      row = ((v & Config::PAGE_MASK) + Config::PAGE_SIZE - 4) / 32;
	    }
	}
      return Redraw;

    case 'D':
      if (Kconsole::console()->find_console(Console::GZIP))
	{
	  Address low_addr, high_addr;

	  Jdb::cursor(Jdb_screen::height(), 27);
	  putchar('[');
	  Jdb::clear_to_eol();

	  if (Jdb_input::get_mword(&low_addr, sizeof(Mword)*2, 16))
	    {
	      putchar('-');
	      if (Jdb_input::get_mword(&high_addr, sizeof(Mword)*2, 16))
		{
		  unsigned l_row = low_addr  / ((cols()-1) * elb);
		  unsigned l_col = (low_addr % ((cols()-1) * elb)) / elb;
		  unsigned h_row = high_addr / ((cols()-1) * elb);

		  if (low_addr <= high_addr)
		    {
		      Mword lines = h_row - l_row;
		      if (lines < 1)
			lines = 1;
		      // enable gzip console
		      Kconsole::console()->
			start_exclusive(Console::GZIP);
		      char old_mode = dump_type;
		      dump_type = D_MODE;
		      draw_table(l_row, l_col, lines, cols());
		      dump_type = old_mode;
		      Kconsole::console()->
			end_exclusive(Console::GZIP);
		    }
		}
	    }
	  return Redraw;
	}
      return Handled;

    case ' ': // change viewing mode
      switch (dump_type)
	{
	case D_MODE: dump_type=B_MODE; return Redraw;
	case B_MODE: dump_type=C_MODE; return Redraw;
	case C_MODE: dump_type=D_MODE; return Redraw;
	}
      break;

    case KEY_TAB:
      show_adapter_memory = !show_adapter_memory;
      return Redraw;

    case KEY_RETURN: // goto address under cursor
    case KEY_RETURN_2:
      if (level<=7 && dump_type==D_MODE)
	{
	  Address virt1;
	  if (Jdb::peek((Address*)virt(row,col), task, virt1))
	    {
	      if (!dump(virt1, task, level +1))
		  return Exit;
	      return Redraw;
	    }
	}
      break;

    case 'u': // disassemble using address the cursor points to
      if (Jdb_disasm::avail() && level<=7 && dump_type == D_MODE)
	{
	  Address virt1;
	  if (Jdb::peek((Address*)virt(row,col), task, virt1))
	    {
	      Jdb::printf_statline("dump", "<CR>=disassemble here",
				   "u[address=" L4_PTR_FMT " task=%p] ",
				   virt1, task);
	      int c1 = Jdb_core::getchar();
	      if (c1 != KEY_RETURN && c1 != ' ' && c != KEY_RETURN_2)
		{
		  Jdb::printf_statline("dump", 0, "u");
		  Jdb::execute_command("u", c1);
		  return Exit;
		}

              return Jdb_disasm::show(virt1, task, level+1, 1)
                     ? Redraw
                     : Exit;
	    }
	}
      return Handled;

    case 'e': // poke memory
      if (dump_type == D_MODE)
	return Edit;
      break;

    case 'c': // set boundaries for highlighting memory contents
      if (level <= 7 && dump_type == D_MODE)
	{
	  Address a;
	  if (Jdb::peek((Address*)virt(row,col), task, a))
	    {
	      const Address pm = 0x100000;
	      highlight_start = (a  > pm)        ? a - pm : 0;
	      highlight_end   = (a <= ~1UL - pm) ? a + pm : ~1UL;
	      return Redraw;
	    }
	}
      break;

    case 'r':
      return Redraw;
    }

  return Handled;

}

PUBLIC
Jdb_module::Action_code
Jdb_dump::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  if (cmd == 0)
    {
      Jdb_module::Action_code code;

      code = Jdb_input_task_addr::action(args, fmt, next_char);
      if (code == Jdb_module::NOTHING)
	{
	  Address addr  = Jdb_input_task_addr::addr();
	  Space *space = Jdb_input_task_addr::space();
	  if (addr == (Address)~0UL)
	    addr = 0;

	  return dump(addr, space, 0);
	}

      if (code != ERROR)
	return code;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_dump::cmds() const
{
  static Cmd cs[] =
    {
      { 0, "d", "dump", "%C",
        "d[t<taskno>|p]<addr>\tdump memory of given/current task at <addr>, or physical",
        &Jdb_input_task_addr::first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_dump::num_cmds() const
{
  return 1;
}

IMPLEMENT
Jdb_dump::Jdb_dump()
  : Jdb_module("INFO"), dump_type(D_MODE)
{}

static Jdb_dump jdb_dump INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

int
jdb_dump_addr_task(Address addr, Space *task, int level)
{ return jdb_dump.dump(addr, task, level); }
