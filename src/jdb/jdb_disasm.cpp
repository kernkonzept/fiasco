INTERFACE:

#include "jdb_module.h"
#include "l4_types.h"
#include "types.h"

class Space;

// ------------------------------------------------------------------------
INTERFACE [!jdb_disasm]:

class Jdb_disasm : public Jdb_module
{
public:
  static bool avail() { return false; }
};

// ------------------------------------------------------------------------
INTERFACE [jdb_disasm]:

class Jdb_disasm : public Jdb_module
{
public:
  Jdb_disasm() FIASCO_INIT;
  static bool avail() { return true; }
private:
  static char show_intel_syntax;
  static char show_lines;
};


// ------------------------------------------------------------------------
IMPLEMENTATION [!jdb_disasm]:

PUBLIC static
Jdb_module::Action_code
Jdb_disasm::show(Address, Space *, int, bool = false)
{
  return Jdb_module::NOTHING;
}

PUBLIC static
bool
Jdb_disasm::show_disasm_line(int, Address &, int, Space *)
{
  return false;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [jdb_disasm]:

#include <cstdio>
#include <cstring>
#include <cstdarg>

#include "disasm.h"
#include "jdb.h"
#include "jdb_bp.h"
#include "jdb_input.h"
#include "jdb_input_task.h"
#include "jdb_lines.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_symbol.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "static_init.h"
#include "task.h"

char Jdb_disasm::show_intel_syntax;
char Jdb_disasm::show_lines = 2;

static
bool
Jdb_disasm::disasm_line(char *buffer, int buflen, Address &addr,
			int show_symbols, Space *task)
{
  int len;

  if ((len = disasm_bytes(buffer, buflen, addr, task, show_symbols,
			  show_intel_syntax, &Jdb::peek_task,
			  &Jdb_symbol::match_addr_to_symbol)) < 0)
    {
      addr += 1;
      return false;
    }

  addr += len;
  return true;
}

static
int
Jdb_disasm::at_symbol(Address addr, Space *task)
{
  return Jdb_symbol::match_addr_to_symbol(addr, task) != 0;
}

static
int
Jdb_disasm::at_line(Address addr, Space *task)
{
  return (show_lines &&
	  Jdb_lines::match_addr_to_line(addr, task, 0, 0, show_lines==2));
}

static
Address
Jdb_disasm::disasm_offset(Address &start, int offset, Space *task)
{
  if (offset>0)
    {
      Address addr = start;
      while (offset--)
	{
	  if (!disasm_line(0, 0, addr, 0, task))
	    {
	      start = addr + offset;
	      return false;
	    }
	  if (at_symbol(addr, task) && !offset--)
	    break;
	  if (at_line(addr, task) && !offset--)
	    break;
	}
      start = addr;
      return true;
    }

  while (offset++)
    {
      Address addr = start-64, va_start;
      for (;;)
	{
	  va_start = addr;
	  if (!disasm_line(0, 0, addr, 0, task))
	    {
	      start += offset-1;
	      return false;
	    }
	  if (addr >= start)
	    break;
	}
      start = va_start;
      if (at_symbol(addr, task) && !offset++)
	break;
      if (at_line(addr, task) && !offset++)
	break;
    }
  return true;
}

PUBLIC static
bool
Jdb_disasm::show_disasm_line(int len, Address &addr,
			     int show_symbols, Space *task)
{
  int clreol = 0;
  if (len < 0)
    {
      len = -len;
      clreol = 1;
    }

  char line[len];
  if (disasm_line(line, len, addr, show_symbols, task))
    {
      if (clreol)
	printf("%.*s\033[K\n", len, line);
      else
	printf("%-*s\n", len, line);
      return true;
    }

  if (clreol)
    puts("........\033[K");
  else
    printf("........%*s", len-8, "\n");
  return false;
}

PUBLIC static
Jdb_module::Action_code
Jdb_disasm::show(Address virt, Space *task, int level, bool do_clear_screen = false)
{
  Address  enter_addr = virt;
  Space *trans_task = Jdb::translate_task(virt, task);

  for (;;)
    {
      if (do_clear_screen)
        Jdb::clear_screen();

      Jdb::cursor();

      Address addr;
      Mword   i;
      for (i=Jdb_screen::height()-1, addr=virt; i>0; i--)
	{
	  const char *symbol;
      	  char str[78], *nl;
	  char stat_str[4] = { "   " };

	  Kconsole::console()->getchar_chance();

	  if ((symbol = Jdb_symbol::match_addr_to_symbol(addr, trans_task)))
	    {
	      snprintf(str, sizeof(str)-2, "<%s", symbol);

	      // cut symbol at newline
	      for (nl=str; *nl!='\0' && *nl!='\n'; nl++)
		;
	      *nl++ = '>';
	      *nl++ = ':';
    	      *nl++ = '\0';
	      
	      printf("%s%s\033[m\033[K\n", Jdb::esc_symbol, str);
	      if (!--i)
		break;
	    }

	  if (show_lines)
	    {
	      if (Jdb_lines::match_addr_to_line(addr, trans_task, str, 
						sizeof(str)-1, show_lines==2))
		{
		  printf("%s%s\033[m\033[K\n", Jdb::esc_line, str);
		  if (!--i)
		    break;
		}
	    }

	  // show instruction breakpoint
          if (Mword i = Jdb_bp::instruction_bp_at_addr(addr))
            {
              stat_str[0] = '#';
              stat_str[1] = '0' + i - 1;
            }

	  printf("%s" L4_PTR_FMT "%s%s  ", 
	         addr == enter_addr ? Jdb::esc_emph : "", addr, stat_str,
		 addr == enter_addr ? "\033[m" : "");
	  show_disasm_line(
#ifdef CONFIG_BIT32
		           -64,
#else
			   -58,
#endif
			   addr, 1, task);
	}

      static char const * const line_mode[] = { "", "[Source]", "[Headers]" };
      static char const * const syntax_mode[] = { "[AT&T]", "[Intel]" };
      Jdb::printf_statline("dis", "<Space>=lines mode",
			   "<" L4_PTR_FMT "> task %-3p  %-9s  %-7s",
			   virt, task, line_mode[(int)show_lines], 
			   syntax_mode[(int)show_intel_syntax]);

      Jdb::cursor(Jdb_screen::height(), 6);
      switch (int c = Jdb_core::getchar())
	{
	case KEY_CURSOR_LEFT:
	case 'h':
	  virt -= 1;
	  break;
	case KEY_CURSOR_RIGHT:
	case 'l':
	  virt += 1;
	  break;
	case KEY_CURSOR_DOWN:
	case 'j':
	  disasm_offset(virt, +1, task);
	  break;
	case KEY_CURSOR_UP:
	case 'k':
	  disasm_offset(virt, -1, task);
	  break;
	case KEY_PAGE_UP:
	case 'K':
	  disasm_offset(virt, -Jdb_screen::height()+2, task);
	  break;
	case KEY_PAGE_DOWN:
	case 'J':
	  disasm_offset(virt, +Jdb_screen::height()-2, task);
	  break;
	case ' ':
	  show_lines = (show_lines+1) % 3;
	  break;
	case KEY_TAB:
	  show_intel_syntax ^= 1;
	  break;
	case KEY_CURSOR_HOME:
	case 'H':
	  if (level > 0)
	    return GO_BACK;
	  break;
	case KEY_ESC:
	  Jdb::abort_command();
	  return NOTHING;
	default:
	  if (Jdb::is_toplevel_cmd(c)) 
	    return NOTHING;
	  break;
	}
    }
  
  return GO_BACK;
}

PUBLIC
Jdb_module::Action_code
Jdb_disasm::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  if (cmd == 0)
    {
      Jdb_module::Action_code code;

      code = Jdb_input_task_addr::action(args, fmt, next_char);
      if (code == Jdb_module::NOTHING
	  && Jdb_input_task_addr::space() != 0)
	{
	  Address addr  = Jdb_input_task_addr::addr();
	  Space *space = Jdb_input_task_addr::space();
	  if (addr == (Address)-1)
	    addr = Jdb::get_entry_frame(Jdb::current_cpu)->ip();
	  return show(addr, space, 0) ? GO_BACK : NOTHING;
	}

      return code;
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_disasm::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "u", "u", "%C",
	  "u[t<taskno>]<addr>\tdisassemble bytes of given/current task addr",
	  &Jdb_input_task_addr::first_char }
    };

  return cs;
}

PUBLIC
int
Jdb_disasm::num_cmds() const
{ return 1; }

IMPLEMENT
Jdb_disasm::Jdb_disasm()
  : Jdb_module("INFO")
{}

static Jdb_disasm jdb_disasm INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
