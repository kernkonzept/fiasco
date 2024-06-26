IMPLEMENTATION:

#include <cstdio>
#include <cstring>

#include "jdb.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "static_init.h"


//===================
// Std JDB modules
//===================

/**
 * Jdb-prompt module.
 *
 * This module handles some commands that
 * change Jdb prompt and screen settings.
 */
class Jdb_pcm : public Jdb_module
{
public:
  Jdb_pcm() FIASCO_INIT;
private:
  static char subcmd;
  static char prompt_color;
  static char direct_enable;
  static int  screen_height;
  static int  screen_width;
  static unsigned input_time_block_sec;

  bool wait_for_escape(Console *cons);
};

char Jdb_pcm::subcmd;
char Jdb_pcm::prompt_color;
char Jdb_pcm::direct_enable;
int  Jdb_pcm::screen_height;
int  Jdb_pcm::screen_width;
unsigned Jdb_pcm::input_time_block_sec;

static Jdb_pcm jdb_pcm INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE
int
Jdb_pcm::get_coords(Console *cons, unsigned &x, unsigned &y)
{
  // A tool like minicom is attached at the other end of the serial line and
  // responds to this "request cursor position" escape sequence.
  cons->write("\033[6n", 4);

  if (!wait_for_escape(cons))
    return 0;

  if (cons->getchar(true) != '[')
    return 0;

  for (y=0; ;)
    {
      int c = cons->getchar(true);
      if (c == ';')
	break;
      if (c < '0' || c > '9')
	return 0;
      y = y*10+c-'0';
    }
  for (x=0; ;)
    {
      int c = cons->getchar(true);
      if (c == 'R')
	break;
      if (c < '0' || c > '9')
	return 0;
      x = x*10+c-'0';
    }
  return 1;
}

PRIVATE
void
Jdb_pcm::detect_screensize()
{
  Console *uart;

  if (!(uart = Kconsole::console()->find_console(Console::UART)))
    return;

  while (uart->getchar(false) != -1)
    ;

  unsigned x, y;
  if (!get_coords(uart, x, y))
    return;

  // set scroll region to the max + set cursor to the max
  uart->write("\033[1;500r\033[500;500H", 18);

  unsigned max_x, max_y;
  if (!get_coords(uart, max_x, max_y))
    return;
  Jdb_screen::set_width(max_x);
  Jdb_screen::set_height(max_y);

  // adapt scroll region and restore cursor
  char str[28];
  snprintf(str, sizeof(str), "\033[1;%ur\033[%u;%uH", max_y, y, x);
  uart->write(str, strlen(str));
}

PUBLIC
Jdb_module::Action_code
Jdb_pcm::action(int cmd, void *&args, char const *&fmt, int &) override
{
  if (cmd)
    return NOTHING;

  if (args == &subcmd)
    {
      switch (subcmd)
        {
        case 'c':
          fmt  = " promptcolor=%c";
          args = &prompt_color;
          return EXTRA_INPUT;
	case 'd':
          if (!Kconsole::console()->find_console(Console::DIRECT))
            {
              printf("\nDirect console not available\n");
              return ERROR;
            }
	  fmt = "%c";
	  args = &direct_enable;
	  return EXTRA_INPUT;
        case 'h':
          fmt  = " screenheight=%d";
          args = &screen_height;
          return EXTRA_INPUT;
        case 'w':
          fmt  = " screenwidth=%d";
          args = &screen_width;
          return EXTRA_INPUT;
	case 'H':
	case 'S':
	  detect_screensize();
	  return NOTHING;
	case 'o':
	  printf("\nConnected consoles:\n");
	  Kconsole::console()->list_consoles();
	  return NOTHING;
        case 'i':
          printf("\nScreen dimensions: %ux%u Cols: %lu\n",
                 Jdb_screen::width(), Jdb_screen::height(),
                 Jdb_screen::cols());
          return NOTHING;
        case 'b':
          args = &input_time_block_sec;
          fmt  = " seconds=%d";
          return EXTRA_INPUT;
        case 'B':
          printf("\nJDB: Ignoring input, enable again by typing 'input'\n");
          Kconsole::console()->set_ignore_input();
          return LEAVE;
	default:
	  return ERROR;
        }
    }
  else if (args == &screen_height)
    {
      // set screen height
      if (24 < screen_height && screen_height < 100)
        Jdb_screen::set_height(screen_height);
    }
  else if (args == &screen_width)
    {
      // set screen height
      if (80 < screen_width && screen_width < 600)
        Jdb_screen::set_width(screen_width);
    }
  else if (args == &prompt_color)
    {
      if (!Jdb::set_prompt_color(prompt_color) )
        {
          putchar(prompt_color);
          puts(" - color expected (lLrRgGbByYmMcCwW)!");
        }
    }
  else if (args == &direct_enable)
    {
      printf(" Direct console %s\n",
	  direct_enable == '+' ? "enabled" : "disabled");
      Jdb_screen::enable_direct(direct_enable == '+');
      if (direct_enable == '+')
	Kconsole::console()->change_state(Console::DIRECT, 0,
					  ~0U, Console::OUTENABLED);
      else
	Kconsole::console()->change_state(Console::DIRECT, 0,
					  ~Console::OUTENABLED, 0);
    }
  else if (args == &input_time_block_sec)
    {
      printf("\nJDB: Ignoring input for %u seconds\n", input_time_block_sec);
      Kconsole::console()->set_ignore_input(input_time_block_sec * 1000000);
      return LEAVE;
    }

  return NOTHING;
}

PUBLIC
int Jdb_pcm::num_cmds() const override
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const * Jdb_pcm::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "J", "Jdb options", "%c",
	   "Jc<color>\tset the Jdb prompt color, <color> must be:\n"
	   "\tnN: noir(black), rR: red, gG: green, bB: blue,\n"
	   "\tyY: yellow, mM: magenta, cC: cyan, wW: white;\n"
	   "\tthe capital letters are for bold text.\n"
	   "Jd{+|-}\ton/off Jdb output to direct console\n"
	   "Jh\tset Jdb screen height\n"
	   "Jw\tset Jdb screen width\n"
	   "JS\tdetect screen size using ESCape sequence ESC [ 6 n\n"
           "Ji\tshow screen information\n"
	   "Jo\tlist attached consoles\n"
           "J{B,b}\tJB: block input, Jb: for specified time",
	   &subcmd }
    };

  return cs;
}

IMPLEMENT
Jdb_pcm::Jdb_pcm()
  : Jdb_module("GENERAL")
{}

IMPLEMENTATION:

#include "processor.h"

// A tool like minicom is attached at the other end of the serial line and
// responds to the magical escape sequence. It may take some time before the
// response arrives if the tool at the other end is attached via network.
IMPLEMENT
bool
Jdb_pcm::wait_for_escape(Console *cons)
{
  // 200ms timeout should be also reasonable for network connections.
  for (unsigned cnt = 4000;; --cnt)
    {
      int c = cons->getchar(false);
      if (c == KEY_ESC || c == KEY_SINGLE_ESC)
        return true;
      if (c != -1 || !cnt)
        return false;
      Delay::udelay(50);
    }
}
