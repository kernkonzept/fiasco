INTERFACE:

#include "types.h"
#include "jdb_types.h"
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
#include "kernel_console.h"
#include "keycodes.h"
#include "simpleio.h"
#include "static_init.h"
#include "types.h"
#include "task.h"
#include "paging_bits.h"

class Jdb_dump : public Jdb_module, public Jdb_table
{
public:
  Jdb_dump() FIASCO_INIT;
  unsigned long cols() const override { return Jdb_screen::cols(); }
  unsigned long rows() const override { return Jdb_screen::rows(); }
  void draw_entry(unsigned long row, unsigned long col) override;
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

PRIVATE template<typename T = Mword> inline
Jdb_addr<T>
Jdb_dump::virt(unsigned long row, unsigned long col)
{
  Address dump_addr = (col-1) * elb + row * (cols()-1) * elb;
  return Jdb_addr<T>(reinterpret_cast<T*>(dump_addr), task);
}

PUBLIC
unsigned
Jdb_dump::col_width(unsigned col) const override
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
Jdb_dump::print_statline(unsigned long row, unsigned long col) override
{
  char const *str = dump_type == D_MODE
                  ? "e=edit u=disasm D=dump <Space>=mode <CR>=goto addr"
                  : "<Space>=mode";
  char s[16];
  Jdb_address a = virt(row, col);
  Jdb::printf_statline("dump", str, "%c<" L4_PTR_FMT "> %s",
                       dump_type, a.addr(),
                       Jdb::addr_space_to_str(a, s, sizeof(s)));
}

IMPLEMENT
void
Jdb_dump::draw_entry(unsigned long row, unsigned long col)
{
  if (col == 0)
    {
      printf("%0*lx", col_width(col), row * (cols()-1) * elb);
      return;
    }

  auto entry = virt(row, col);

  // prevent apic from getting confused by invalid register accesses
  if (&ignore_invalid_apic_reg_access)
    ignore_invalid_apic_reg_access = 1;

  if (!Jdb::is_adapter_memory(entry) || show_adapter_memory)
    {
      Mword mword;
      if (Jdb::peek(entry, mword))
        {
	  if (dump_type==D_MODE)
	    {
	      if (mword == 0)
          printf("%*lu", Jdb_screen::Mword_size_bmode, mword);
	      else if (mword == Invalid_address)
          printf("%*d", Jdb_screen::Mword_size_bmode, -1);
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
      else // !mapped
        printf("%.*s",
               dump_type == C_MODE ? Jdb_screen::Mword_size_cmode
                                   : Jdb_screen::Mword_size_bmode,
               Jdb_screen::Mword_not_mapped);
    }
  else // is_adapter_memory
    printf("%.*s",
           dump_type == C_MODE ? Jdb_screen::Mword_size_cmode
                               : Jdb_screen::Mword_size_bmode,
           Jdb_screen::Mword_adapter);

  if (&ignore_invalid_apic_reg_access)
    ignore_invalid_apic_reg_access = 0;
}

PUBLIC
Jdb_module::Action_code
Jdb_dump::dump(Jdb_address virt, int level)
{ //printf("DU: %p %lx\n", task, virt); Jdb::getchar();
  int old_l = this->level;
  this->level = level;
  this->task = virt.space();
  dump_type = D_MODE;
  if (!level)
    Jdb::clear_screen();

  unsigned long row =  virt.addr() / ((cols()-1) * elb);
  unsigned long col = (virt.addr() % ((cols()-1) * elb)) / elb + 1;
  bool r = show(row, col);
  this->level = old_l;
  return r ? EXTRA_INPUT : NOTHING;
}

PUBLIC
bool
Jdb_dump::edit_entry(unsigned long row, unsigned long col, unsigned cx, unsigned cy) override
{
  Jdb_addr<Mword> entry = virt(row,col);

  Mword mword;
  if (!Jdb::peek(entry, mword))
    return false;

  putstr(Jdb_screen::Mword_blank);
  Jdb::printf_statline("dump", "<CR>=commit change",
		       "edit <" L4_PTR_FMT "> = " L4_PTR_FMT, entry.addr(), mword);

  Jdb::cursor(cy, cx);
  Mword new_mword;
  if (Jdb_input::get_mword(&new_mword, sizeof(Mword)*2, 16))
    Jdb::poke(entry, new_mword);

  return true; // redraw
}

PUBLIC
unsigned
Jdb_dump::key_pressed(int c, unsigned long &row, unsigned long &col) override
{
  switch (c)
    {
    default:
      return Nothing;

    case KEY_CURSOR_HOME: // return to previous or go home
      if (level == 0)
        {
          Jdb_addr<Mword> v = virt(row, col);
          if (v.is_null())
            return Handled;

          if (Pg::offset(v.addr()) == 0)
            row -= Config::PAGE_SIZE / 32;
          else
            {
              col = 1;
              row = Pg::trunc(v.addr()) / 32;
            }
          return Redraw;
        }
      return Back;

    case KEY_CURSOR_END:
        {
          Jdb_addr<Mword> v = virt(row, col);
          if (Pg::offset(v.addr()) >> 2 == 0x3ff)
            row += Config::PAGE_SIZE / 32;
          else
            {
              col = Jdb_screen::cols() - 1;
              row = (Pg::trunc(v.addr()) + (Config::PAGE_SIZE - 4)) / 32;
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
          Jdb_addr<Address> a = virt<Address>(row, col);
	  if (Jdb::peek(a, virt1))
	    {
	      if (!dump(Jdb_address(virt1, a.space()), level + 1))
		  return Exit;
	      return Redraw;
	    }
	}
      break;

    case 'u': // disassemble using address the cursor points to
      if (Jdb_disasm::avail() && level<=7 && dump_type == D_MODE)
	{
	  Address virt1;
          Jdb_addr<Address> a = virt<Address>(row, col);
	  if (Jdb::peek(a, virt1))
	    {
	      char s[16];
	      Jdb::printf_statline("dump", "<CR>=disassemble here",
				   "u[address=" L4_PTR_FMT "%s] ", virt1,
				   Jdb::addr_space_to_str(a, s, sizeof(s)));
	      int c1 = Jdb_core::getchar();
	      if (c1 != KEY_RETURN && c1 != ' ' && c != KEY_RETURN_2)
		{
		  Jdb::printf_statline("dump", nullptr, "u");
		  Jdb::execute_command("u", c1);
		  return Exit;
		}

              return Jdb_disasm::show(Jdb_address(virt1, a.space()), level + 1)
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
	  if (Jdb::peek(virt<Address>(row, col), a))
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
Jdb_dump::action(int cmd, void *&args, char const *&fmt, int &next_char) override
{
  if (cmd == 0)
    {
      Jdb_module::Action_code code;

      code = Jdb_input_task_addr::action(args, fmt, next_char);
      if (code == Jdb_module::NOTHING)
        return dump(Jdb_input_task_addr::address(), 0);

      if (code != ERROR)
        return code;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_dump::cmds() const override
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
Jdb_dump::num_cmds() const override
{
  return 1;
}

IMPLEMENT
Jdb_dump::Jdb_dump()
  : Jdb_module("INFO"), dump_type(D_MODE)
{}

static Jdb_dump jdb_dump INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

int
jdb_dump_addr_task(Jdb_address addr, int level)
{ return jdb_dump.dump(addr, level); }
