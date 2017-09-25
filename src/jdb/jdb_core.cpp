INTERFACE:

#include "static_init.h"
#include "jdb_module.h"

/**
 * The core of the modularized Jdb.
 * @see Jdb_module
 * @see Jdb_category
 *
 * This class provides the core functions for handling
 * Jdb_modules and providing them with the right input.
 *
 */
class Jdb_core
{
public:

  /**
   * The command structure for Jdb_core.
   *
   * This structure consists of a pointer to the Jdb_module
   * and a Jdb_module::Cmd structure. It is used in exec_cmd()
   * and returned from has_cmd().
   */
  struct Cmd
  {
    /**
     * Pointer to the module providing this command.
     */
    Jdb_module            *mod;

    /**
     * The Jdb_module::Cmd structure, describing the command.
     *
     * If this is a null pointer the command is invalid.
     * @see Jdb_module
     * @see Jdb_module::Cmd
     */
    Jdb_module::Cmd const *cmd;

    /**
     * Create a Jdb_core::Cmd.
     * @param _mod the Jdb_module providing the command.
     * @param _cmd the command structure (see Jdb_module::Cmd).
     */
    Cmd(Jdb_module *_mod, Jdb_module::Cmd const *_cmd = 0)
      : mod(_mod), cmd(_cmd)
    {}
  };

  /**
   * Get the command structure accoring to the given name.
   * @param cmd the command to look for.
   * @return A valid Cmd structure if cmd was found, or a
   *         Cmd structure where Cmd::cmd is a null pointer if
   *         no module provides such a command.
   */
  static Cmd has_cmd(char const *cmd);

  /**
   * Execute the command according to cmd.
   * @param cmd the command structure (see Jdb_core::Cmd), which
   *        describes the command to execute.
   * @return 0 if Jdb_module::action() returned LEAVE
   *         1 if Jdb_module::action() returned NOTHING
   *         2 if Jdb_module::action() returned GO_BACK (KEY_HOME entered)
   *         3 if the input was aborted (KEY_ESC entered) or was invalid
   *
   * This method is actually responsible for reading the input
   * with respect to the commands format string and calling
   * the Jdb_module::action() method after that.
   *
   */
  static int exec_cmd(Cmd const cmd, char const *str, int push_next_char = -1);

  /**
   * Overwritten getchar() to be able to handle next_char.
   */
  static int getchar(void);

  /**
   * Call this function every time a `\n' is written to the
   *        console and it stops output when the screen is full.
   * @return 0 if user wants to abort the output (escape or 'q' pressed)
   */
  static int new_line(unsigned &line);

  static void prompt_start();
  static void prompt_end();
  static void prompt();
  static int  prompt_len();
  static void update_prompt();
  static int set_prompt_color(char v);

  /**
   * Like strlen but do not count ESC sequences.
   */
  static int print_len(const char *s);

  static char esc_prompt[];

  static void (*wait_for_input)();

public:
  static bool short_mode;
  typedef int (Input_fmt)(char fmt, int *size, char const *cmd_str, void *buf);

private:
  static int  next_char;
  static Input_fmt *_fmt_list[26];
};

#define JDB_ANSI_black        "30"
#define JDB_ANSI_gray         "30;1"
#define JDB_ANSI_red          "31"
#define JDB_ANSI_lightred     "31;1"
#define JDB_ANSI_green        "32"
#define JDB_ANSI_lightgreen   "32;1"
#define JDB_ANSI_brown        "33"
#define JDB_ANSI_yellow       "33;1"
#define JDB_ANSI_blue         "34"
#define JDB_ANSI_lightblue    "34;1"
#define JDB_ANSI_magenta      "35"
#define JDB_ANSI_lightmagenta "35;1"
#define JDB_ANSI_cyan         "36"
#define JDB_ANSI_lightcyan    "36;1"
#define JDB_ANSI_white        "37"
#define JDB_ANSI_brightwhite  "37;1"
#define JDB_ANSI_default      ""

#define JDB_ANSI_COLOR(color)   "\033[" JDB_ANSI_##color "m"

#define JDB_ANSI_END          "\033[m"


IMPLEMENTATION:

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <simpleio.h>

#include "div32.h"
#include "l4_types.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "jdb_prompt_ext.h"
#include "jdb_screen.h"
#include "processor.h"

bool Jdb_core::short_mode = true;
int  Jdb_core::next_char  = -1;
char Jdb_core::esc_prompt[32] = JDB_ANSI_COLOR(green);
Jdb_core::Input_fmt *Jdb_core::_fmt_list[26]; 
void (*Jdb_core::wait_for_input)();

PUBLIC static
bool
Jdb_core::add_fmt_handler(char fmt, Input_fmt* hdlr)
{
  if (fmt < 'a' || (fmt - 'a') >= (int)(sizeof(_fmt_list)/sizeof(_fmt_list[0])))
    return false;

  if (_fmt_list[fmt - 'a'])
    return false;

  _fmt_list[fmt - 'a'] = hdlr;
  return true;
}

PUBLIC static
void
Jdb_core::print_prompt()
{ prompt_start(); prompt(); prompt_end(); }

IMPLEMENT
void Jdb_core::update_prompt()
{
  Jdb_prompt_ext::update_all();
}

IMPLEMENT
void Jdb_core::prompt_start()
{
  putstr(esc_prompt);
}

IMPLEMENT
void Jdb_core::prompt_end()
{
  putstr(JDB_ANSI_END);
}

IMPLEMENT
void Jdb_core::prompt()
{
  Jdb_prompt_ext::do_all();
  if (short_mode)
    putstr("jdb: ");
  else
    putstr("jdb# ");
}

IMPLEMENT
int Jdb_core::prompt_len()
{
  return 5;
}

IMPLEMENT
int Jdb_core::print_len(const char *s)
{
  int l = 0;
  while (*s)
    {
      if (s[0] == '\033' && s[1] == '[')
        {
	  s += 2;
	  while (*s && *s != 'm')
	    s++;
	  if (*s)
	    s++; // skip 'm'
        }
      else
        {
	  l++;
          s++;
	}
    }
  return l;
}

PUBLIC static
int
Jdb_core::get_ansi_color(char c)
{
  switch(c)
    {
    case 'N': case 'n': return 30;
    case 'R': case 'r': return 31;
    case 'G': case 'g': return 32;
    case 'Y': case 'y': return 33;
    case 'B': case 'b': return 34;
    case 'M': case 'm': return 35;
    case 'C': case 'c': return 36;
    case 'W': case 'w': return 37;
    default:  return 0;
    }
}


IMPLEMENT
int Jdb_core::set_prompt_color(char x)
{
  unsigned pc = get_ansi_color(x);

  if (pc == 0)
    return 0;

  if (x >= 'A' && x <= 'Z')
    snprintf(esc_prompt, sizeof(esc_prompt) - 1, "\033[%u;%dm", pc, 1);
  else
    snprintf(esc_prompt, sizeof(esc_prompt) - 1, "\033[%um", pc);

  return 1;

}

IMPLEMENT
Jdb_core::Cmd Jdb_core::has_cmd(char const *cmd)
{
  for (Jdb_module::List::Const_iterator m = Jdb_module::modules.begin();
       m != Jdb_module::modules.end(); ++m)
    {
      Cmd c(*m);
      c.cmd = m->has_cmd(cmd, short_mode);
      if (c.cmd)
        return c;
    }

  return Cmd(0);
}

PRIVATE static
unsigned
Jdb_core::match_len(char const *a, char const *b, unsigned l)
{
  unsigned p = 0;
  while (*a && *b && p < l && *a == *b)
    {
      ++a; ++b; ++p;
    }
  return p;
}

PUBLIC static
unsigned
Jdb_core::print_alternatives(char const *prefix)
{
  unsigned prefix_len = 0;
  char const *match = 0;
  typedef Jdb_module::List::Const_iterator Iter;
  for (Iter m = Jdb_module::modules.begin(); m != Jdb_module::modules.end(); ++m)
    {
      unsigned sc_max = m->num_cmds();
      Jdb_module::Cmd const *cmds = m->cmds();
      for (unsigned sc = 0; sc < sc_max; ++sc)
	{
	  if (!Jdb_module::match(cmds[sc].cmd, prefix, false))
	    continue;

	  if (!match)
	    {
	      match = cmds[sc].cmd;
	      prefix_len = strlen(match);
	    }
	  else
	    prefix_len = match_len(match, cmds[sc].cmd, prefix_len);
	  printf("%s %s\n", cmds[sc].cmd, cmds[sc].fmt);
	}
    }
  return prefix_len;
}

PUBLIC static
Jdb_core::Cmd
Jdb_core::complete_cmd(char const *prefix, bool &multi_match)
{
  Cmd match(0,0);
  multi_match = false;
  typedef Jdb_module::List::Const_iterator Iter;
  for (Iter m = Jdb_module::modules.begin(); m != Jdb_module::modules.end(); ++m)
    {
      unsigned sc_max = m->num_cmds();
      Jdb_module::Cmd const *cmds = m->cmds();
      for (unsigned sc = 0; sc < sc_max; ++sc)
	{
	  if (!Jdb_module::match(cmds[sc].cmd, prefix, false))
	    continue;

	  if (match.cmd)
	    multi_match = true;
	  else
	    match = Cmd(*m, cmds + sc);
	}
    }

  return match;
}

IMPLEMENT
int Jdb_core::getchar()
{
  if (next_char != -1)
    {
      int c = next_char;
      next_char = -1;
      return c;
    }

  int c;

  while (1)
    {
      c = Kconsole::console()->getchar(false);
      if (c != -1)
        return c;

      if (wait_for_input)
        wait_for_input();
      else
        Proc::pause();
    }
}

PUBLIC static
int
Jdb_core::cmd_getchar(char const *&str)
{
  if (next_char != -1)
    {
      int c = next_char;
      next_char = -1;
      return c;
    }

  if (short_mode)
    return getchar();

  if (!str)
    return KEY_RETURN;

  if (!*str)
    return KEY_RETURN;

  return *(str++);
}

PUBLIC static inline NEEDS[<cstdio>]
void
Jdb_core::cmd_putchar(int c)
{ if (short_mode) putchar(c); }

IMPLEMENT
int Jdb_core::exec_cmd(Cmd const cmd, char const *str, int push_next_char = -1)
{
  char const* f = cmd.cmd->fmt;
  char const* f1;

  //char args[256];
  void *argbuf = (void*)cmd.cmd->argbuf;

  enum {
    NORMAL,
    UNSIGNED,
    MULTI,
  } num_mode;

  int num_base = 10;
  int num_pos = 0, num_digit = 0;
  int max_len = 0, max_digit = 0;
  int c, cv;
  char fm;

  next_char = push_next_char;

  do {

    char *next_arg = (char*)argbuf;
    char const *old_f = f;
    while(*f)
      {
	f1 = f;

	while(*f && *f!='%')
	  ++f;

        if (short_mode)
	  putnstr( f1, (f-f1) );

	if(*(f++))
	  {
	    int long_fmt = sizeof(long long int) == sizeof(void*);
	    bool negative = false;
	    long long int val = 0;
	    max_len = 0;

	  next_fmt:
	    if(*(f))
	      switch((fm=*(f++)))
		// Attention: Each case statement must finish with "continue"
		//            else it falls through to int_done!
		{
		case '0': case '1': case '2':
		case '3': case '4': case '5':
		case '6': case '7': case '8':
		case '9':
		  max_len = max_len * 10 + fm - '0';
		  goto next_fmt;
		case '%':
		  cmd_putchar('%');
		  continue;

		case 'l':
		  long_fmt++;
		  if(long_fmt > 2) 
		    long_fmt = 2;
		  goto next_fmt;

		case 'h':
		  long_fmt--;
		  if(long_fmt < -1)
		    long_fmt = -1;
		  goto next_fmt;

		case 'i':
		  num_base = 10;
		  num_mode = MULTI;
		  goto input_num;

		case 'p':
		  num_base = 16;
		  num_mode = UNSIGNED;
		  if(sizeof(short int)==sizeof(void*))
		    long_fmt = -1;
		  else if(sizeof(int)==sizeof(void*))
		    long_fmt = 0;
		  else if(sizeof(long int)==sizeof(void*))
		    long_fmt = 1;
		  else //if(sizeof(long long int)==sizeof(void*))
		    long_fmt = 2;
		  goto input_num;

		case 'o':
		  cmd_putchar('0');
		  num_mode = UNSIGNED;
		  num_base = 8;
		  goto input_num;

		case 'X':
		case 'x':
		  num_mode = UNSIGNED;
		  num_base = 16;
		  goto input_num;

		case 'd':
		  num_mode = NORMAL;
		  num_base = 10;
		  goto input_num;

		case 'u':
		  num_mode = UNSIGNED;
		  num_base = 10;
		  goto input_num;

		input_num:
		  num_pos = num_digit = 0;
		  max_digit = 0;
		  if (num_base == 16)
		    {
		      if (long_fmt == -1)
			max_digit = 2*sizeof(short int);
		      else if (long_fmt == 0)
			max_digit = 2*sizeof(int);
		      else if (long_fmt == 1)
			max_digit = 2*sizeof(long int);
		      else
			max_digit = 2*sizeof(long long int);
		    }
		  while((c = cmd_getchar(str)) != ' ' && c!=KEY_RETURN
                         && c != KEY_RETURN_2)
		    {
		      if(c==KEY_ESC)
			return 3;
		      
		      if(c==KEY_BACKSPACE && num_pos>0)
			{
			  putstr("\b \b");
			  if(num_pos == 1 && negative)
			    negative = false;
			  else if(num_pos==1 && num_mode==MULTI && num_base==8)
			    num_base = 10;
			  else if(num_pos==2 && num_mode==MULTI && num_base==16)
			    num_base = 8;
			  else
			    {
			      val = div32(val, num_base);
			      num_digit--;
			    }

			  num_pos--;
			  continue;
			}
		      else if(num_mode!=UNSIGNED && num_pos==0 && c=='-')
			{
			  num_pos++;
			  cmd_putchar(c);
			  negative = true;
			  continue;
			}
		      else if(num_mode==MULTI && num_pos==0 && c=='0')
			{
			  num_pos++;
			  cmd_putchar(c);
			  num_base = 8;
			  continue;
			}
		      else if(num_mode==MULTI && num_pos==1 && num_base==8 
			      && (c=='x' || c=='X'))
			{
			  num_pos++;
			  cmd_putchar(c);
			  num_base = 16;
			  continue;
			}
		      else if(num_pos==1 && (c=='x' || c=='X'))
			{
			  // ignore 0x to allow direct pasting of addresses
			  num_pos--;
			  num_digit--;
			  putstr("\b \b");
			  continue;
			}
		      else if(c>='0' && c<='9')
			{
			  cv = c-'0';
			}
		      else if((c|0x20)>='a' && (c|0x20)<='f')
			{
			  cv = (c|0x20) - 'a' + 10;
			}
		      else
			continue;

		      if(cv < num_base)
			{
			  num_pos++;
			  num_digit++;
			  cmd_putchar(c);
			  val = val * num_base + cv;
			  if ((max_len   != 0 && num_pos   >= max_len) ||
			      (max_digit != 0 && num_digit >= max_digit))
			    break;
			}
		    }
		  if (num_pos == 0)
		    return 3;
		  goto int_done;

		case 't': printf("%%t is not supported...\n");
		  continue;

		case 'C':
		case 'c':
		  {
		    int c = cmd_getchar(str);
		    if(c==KEY_ESC)
		      return 3;

		    if(fm == 'c' && isprint(c))
		      cmd_putchar(c);
		    if(next_arg)
		      *next_arg++ = c;
		  }
		  continue;

		case 's':
		case 'S':
		  if(!max_len)
		    continue;

		  num_pos = 0;
		  while((c = cmd_getchar(str)) != KEY_RETURN && c!=' '
                        && c != KEY_RETURN_2)
		    {
		      if(c==KEY_ESC)
			return 3;

		      if(c==KEY_BACKSPACE && num_pos)
			{
			  putstr("\b \b");
			  num_pos--;
			}
		      else if(isprint(c) && (num_pos+1 < max_len))
			{
			  cmd_putchar(c);
			  next_arg[num_pos++] = c;
			}

		      next_arg[num_pos] = '\0';

		      if (fm=='S')
			{
			  int oldlen = num_pos;
			  (**(Jdb_module::Gotkey**)(next_arg+max_len))
			    (next_arg, max_len, c);
			  int newlen = strlen(next_arg);
			  if (newlen > oldlen)
			    printf("%s", next_arg + oldlen);
			  else if (newlen < oldlen)
			    for (int i=newlen; i<oldlen; i++)
			      putstr("\b \b");
			  num_pos = newlen;
			}
		    }
		  next_arg[num_pos] = '\0';
		  next_arg[max_len-1] = 0;
		  next_arg += max_len;
		  continue;

		default:
		  if (fm > 'a'
		      && (fm - 'a') < (int)(sizeof(_fmt_list)/sizeof(_fmt_list[0]))
		      && _fmt_list[fm -'a'])
		    {
		      int size = sizeof(int);
		      switch(long_fmt)
			{
			default: break;
			case 1: size = sizeof(long int); break;
			case 2: size = sizeof(long long int);  break;
			case -1: size = sizeof(short int); break;
			}
		      int res = (_fmt_list[fm - 'a'])(fm, &size, str, next_arg);
		      if (res)
			return res;

		      next_arg += size;
		      continue;
		    }

		  puts(" unknown format! ");
		  return 3;
		}

	  int_done:
	    if(negative) val = -val;

	    if(next_arg)
	      switch(long_fmt)
		{
		default:
		    {
		      int *v = (int*)next_arg;
		      *v = val;
		      next_arg += sizeof(int);
		    }
		  break;
		case 1:
		    {
		      long int *v = (long int*)next_arg;
		      *v = val;
		      next_arg += sizeof(long int);
		    }
		  break;
		case 2:
		    {
		      long long int *v = (long long int*)next_arg;
		      *v = val;
		      next_arg += sizeof(long long int);
		    }
		  break;
		case -1:
		    {
		      short int *v = (short int*)next_arg;
		      *v = val;
		      next_arg += sizeof(short int);
		    }
		  break;
		}
	  }
	else
	  break;
      }

    f = old_f;

    switch (cmd.mod->action(cmd.cmd->id, argbuf, f, next_char))
      {
      case Jdb_module::EXTRA_INPUT:
	// more input expected
	next_char = -1;
	// fall through
      case Jdb_module::EXTRA_INPUT_WITH_NEXTCHAR:
	// more input expected, also consider previously entered key
	break;
      case Jdb_module::LEAVE:
	// leave kernel debugger
	return 0;
      case Jdb_module::GO_BACK:
	// user entered KEY_HOME
	return 2;
      case Jdb_module::NOTHING:
	// finished successfully
	return 1;
      default:
	// there was an error
	return 3;
      }
  } while(1);

}

IMPLEMENT
int
Jdb_core::new_line( unsigned &line )
{
  if (line++ > Jdb_screen::height()-3)
    {
      putstr("--- CR: line, SPACE: page, ESC: abort ---");
      int a = getchar();
      putstr("\r\033[K");

      switch (a)
	{
	case KEY_ESC:
	case 'q':
	case '^':
	  cmd_putchar('\n');
	  return 0;
	case KEY_RETURN:
	case KEY_RETURN_2:
	  line--;
	  return 1;
	default:
	  line=0;
	  return 1;
	}
    }
  return 1;
}


//===================
// Std JDB modules
//===================


/**
 * Private 'go' module.
 * 
 * This module handles the 'go' or 'g' command 
 * that continues normal program execution.
 */
class Go_m : public Jdb_module
{
public:
  Go_m() FIASCO_INIT;
};

static Go_m go_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

IMPLEMENT
Go_m::Go_m()
  : Jdb_module("GENERAL")
{}

PUBLIC
Jdb_module::Action_code Go_m::action( int, void *&, char const *&, int & )
{
  return LEAVE;
}

PUBLIC
int Go_m::num_cmds() const
{ 
  return 1;
}

PUBLIC
Jdb_module::Cmd const * Go_m::cmds() const
{
  static Cmd cs[] =
    { 
	{ 0, "g", "go", "",
	   "g\tleave kernel debugger\n"
	   "Return\tshow debug message", 0 },
    };

  return cs;
}


/**
 * Private 'help' module.
 * 
 * This module handles the 'help' or 'h' command and 
 * prints out a help screen.
 */
class Help_m : public Jdb_module
{
public:
  Help_m() FIASCO_INIT;
};


static Help_m help_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO);


PUBLIC
Jdb_module::Action_code Help_m::action( int, void *&, char const *&, int & )
{
  size_t const tab_width = 27;

  if(Jdb_category::categories.begin() == Jdb_category::categories.end())
    {
      printf("No debugger commands seem to have been registered\n");
      return NOTHING;
    }

  unsigned line = 0;

  puts("");
  for (Jdb_category::List::Const_iterator c = Jdb_category::categories.begin();
       c != Jdb_category::categories.end(); ++c)
    {
      bool first = true;
      for (Jdb_module::List::Const_iterator m = Jdb_module::modules.begin();
           m != Jdb_module::modules.end(); ++m)
	{
          if (m->category() != *c)
            continue;

	  if(first)
	    {
	      if(!Jdb_core::new_line(line))
		return NOTHING;
	      printf("\033[1m[%s]\033[0m %s", c->name(), c->description());
	      putchar('\n');
	      if(!Jdb_core::new_line(line))
		return NOTHING;
	      first = false;
	    }
	  unsigned ncmds = m->num_cmds();
	  Jdb_module::Cmd const *cmds = m->cmds();
	  for(unsigned x = 0; x < ncmds; x++)
	    {
	      char const *descr = cmds[x].descr;
	      if (!descr)
		continue;
	      size_t pos = strcspn(descr, "\n\t");
	      size_t xpos = 2;
	      putstr("  ");
	      while( pos  < strlen(descr))
		{
		  putnstr(descr, pos);
		  xpos += pos;
		  switch(descr[pos])
		    {
		    case '\n':
		      putchar('\n');
		      if(!Jdb_core::new_line(line))
			return NOTHING;
		      putstr("  ");
		      xpos = 2;
		      break;
		    case '\t':
		      if(xpos<tab_width)
			{
			  putnstr("                            ",
				  tab_width-xpos);
			  xpos = tab_width;
			}
		      else
			{
			  putchar(' ');
			  xpos++;
			}
		      break;
		    default:
		      break;
		    }
		  descr = descr + pos + 1;
		  pos = strcspn(descr, "\n\t");
		}

	      putstr(descr);
	      putchar('\n');
	      if(!Jdb_core::new_line(line))
		return NOTHING;
	    }
	}
    }

  putchar('\n');

  return NOTHING;
}

PUBLIC
int Help_m::num_cmds() const
{ 
  return 2;
}

PUBLIC
Jdb_module::Cmd const * Help_m::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "h", "help", "", "h\tShow this help screen.", 0 },
	{ 0, "?", 0, "", 0, 0 },
    };

  return cs;
}

IMPLEMENT
Help_m::Help_m()
  : Jdb_module("GENERAL")
{}

#define CAT(n, x...) static Jdb_category INIT_PRIORITY(JDB_CATEGORY_INIT_PRIO) n(x)
CAT(a, "GENERAL",    "general debugger commands",      0);
CAT(b, "INFO",       "information about kernel state", 1);
CAT(c, "MONITORING", "monitoring kernel events",       2);
CAT(d, "DEBUGGING",  "real debugging stuff",           3);
#undef CAT
