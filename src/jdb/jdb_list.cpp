INTERFACE:

#include "string_buffer.h"

class Jdb_list
{
public:
  virtual char const *get_mode_str() const { return "[std mode]"; }
  virtual void next_mode() {}
  virtual void next_sort() {}
  virtual void *get_head() const = 0;
  virtual void show_item(String_buffer *buffer, void *item) const = 0;
  virtual char const *show_head() const = 0;
  virtual int seek(int cnt, void **item) = 0;
  virtual bool enter_item(void * /*item*/) const { return true; }
  virtual void *follow_link(void *a) { return a; }
  virtual bool handle_key(void * /*item*/, int /*keycode*/) { return false; }
  virtual void *parent(void * /*item*/) { return 0; }
  virtual void *get_valid(void *a) { return a; }

private:
  typedef String_buf<256> Line_buf;
  void *_start, *_last;
  void *_current;
  char _filter_str[20];
};


// ---------------------------------------------------------------------------
IMPLEMENTATION:

#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_core.h"
#include "jdb_input.h"
#include "jdb_regex.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "simpleio.h"
#include <minmax.h>



PUBLIC
Jdb_list::Jdb_list()
  : _start(0), _current(0)
{
  _filter_str[0] = 0;
}

// set _t_start element of list
PUBLIC
void
Jdb_list::set_start(void *start)
{
  _start = start;
}

// _t_start-- if possible
PUBLIC inline
bool
Jdb_list::line_back()
{ return filtered_seek(-1, &_start); }

// _t_start++ if possible
PUBLIC inline
bool
Jdb_list::line_forw()
{
  if (filtered_seek(1, &_last))
    return filtered_seek(1, &_start);
  else
    return false;
}
#if 0
  Thread *t = _t_start;
  iter(+Jdb_screen::height()-2, &_t_start);
  iter(-Jdb_screen::height()+3, &_t_start);
  return t != _t_start;
}
#endif

// _t_start -= 24 if possible
PUBLIC
bool
Jdb_list::page_back()
{ return filtered_seek(-Jdb_screen::height()+2, &_start); }

// _t_start += 24 if possible
PUBLIC
bool
Jdb_list::page_forw()
{
  int fwd = filtered_seek(Jdb_screen::height()-2, &_last);
  if (fwd)
    return filtered_seek(fwd, &_start);
  return false;
}

#if 0
  Thread *t = _t_start;
  iter(+Jdb_screen::height()*2-5, &_t_start);
  iter(-Jdb_screen::height()  +3, &_t_start);
  return t != _t_start;
}
#endif

// _t_start = first element of list
PUBLIC
bool
Jdb_list::goto_home()
{ return filtered_seek(-99999, &_start); }

// _t_start = last element of list
PUBLIC
bool
Jdb_list::goto_end()
{ return filtered_seek(99999, &_start); }
#if 0
  Thread *t = _t_start;
  iter(+9999, &_t_start);
  iter(-Jdb_screen::height()+2, &_t_start);
  return t != _t_start;
}
#endif

// search index of search starting from _start
PRIVATE
int
Jdb_list::lookup_in_visible_area(void *search)
{
  unsigned i;
  void *t;

  for (i = 0, t = _start; i < Jdb_screen::height() - 3; ++i)
    {
      if (t == search)
        return i;

      filtered_seek(1, &t);
    }

  return -1;
}

// get y'th element of thread list starting from _t_start
PRIVATE
void *
Jdb_list::index(int y)
{
  void *t = _start;

  filtered_seek(y, &t);
  return t;
}

PRIVATE
void
Jdb_list::handle_string_filter_input()
{
  Jdb::printf_statline("filter", 0, "%s=%s",
                       Jdb_regex::avail() ? "Regexp" : "Search",
                       _filter_str);

  Jdb::cursor(Jdb_screen::height(), 16 + strlen(_filter_str));
  if (!Jdb_input::get_string(_filter_str, sizeof(_filter_str)) ||
      !_filter_str[0])
    return;


  if (Jdb_regex::avail() && !Jdb_regex::start(_filter_str))
    {
      _filter_str[0] = 0;
      Jdb::printf_statline("search", 0, "Error in regexp");
      Jdb::getchar();
    }
}

PRIVATE
Jdb_list::Line_buf *
Jdb_list::render_visible(void *i)
{
  static Line_buf buffer;
  buffer.clear();
  void *p = i;
  while ((p = parent(p)))
    buffer.append(' ');

  show_item(&buffer, i);
  if (_filter_str[0])
    {
      buffer.terminate();
      if (Jdb_regex::avail())
        {
          if (!Jdb_regex::find(buffer.begin(), 0, 0))
            i = 0;
        }
      else if (!strstr(buffer.begin(), _filter_str))
        i = 0;
    }

  if (i)
    return &buffer;

  return 0;
}

PRIVATE
int
Jdb_list::print_limit(const char *s, int visible_len)
{
  if (!s || !visible_len)
    return 0;

  int s_len = 0, e = 0;
  while (*s && visible_len)
    {
      if (e == 0 && *s == '\033')
        e = 1;
      else if (e == 1 && *s == '[')
        e = 2;
      else if (e == 2)
        {
          if (isalpha(*s))
            e = 0;
        }
      else
        visible_len--;

      s_len++;
      s++;
    }

  return s_len;
}

PRIVATE
void
Jdb_list::show_line(Jdb_list::Line_buf *b)
{
  Kconsole::console()->getchar_chance();

  // our modified printf ignores the length argument if used with
  // strings containing ESC-sequences
  int s_len_visible = print_limit(b->begin(), Jdb_screen::width());
  b->begin()[s_len_visible] = 0;
  printf("%s\033[K\n", b->begin());
}

PRIVATE
void *
Jdb_list::get_visible(void *i)
{
  if (render_visible(i))
    return i;

  filtered_seek(1, &i);

  return i;
}

PRIVATE
int
Jdb_list::filtered_seek(int cnt, void **item, Jdb_list::Line_buf **buf = 0)
{
  if (cnt == 0)
    return 0;

  int c = 0;
  int d = cnt < 0 ? -1 : 1;
  for (;;)
    {
      int i;

      if ((i = seek(d, item)) == 0)
        return c;

      if (Line_buf *b = render_visible(*item))
        {
          if (buf)
            *buf = b;
          c += d;
          if (cnt == c)
            return c;
        }
    }
}

// show complete page using show callback function
PUBLIC
int
Jdb_list::page_show()
{
  void *t = _start;
  unsigned i = 0;

  if (Jdb_regex::avail() && _filter_str[0])
    assert(Jdb_regex::start(_filter_str));

  if (!t)
    return 0;

  Line_buf *b = render_visible(t);

  for (i = 0; i < Jdb_screen::height()-3; ++i)
    {
      if (!t)
        break;
      _last = t;

      if (b)
        show_line(b);

      if (!filtered_seek(1, &t, &b))
        return i;
    }

  return i - 1;
}

#if 0
// show complete list using show callback function
PRIVATE
int
Jdb_list::complete_show()
{
  void *t = _start;
  int i = 0;
  for (i = 0; ; ++i, seek(1, &t))
    {
      if (!t)
	break;

      show_line(t);
    }

  return i;
}

PUBLIC
Jdb_module::Action_code
Jdb_thread_list::action(int cmd, void *&argbuf, char const *&fmt, int &)
{
  static char const *const cpu_fmt = " cpu=%i\n";
  static char const *const nfmt = "\n";
  if (cmd == 0)
    {
      if (fmt != cpu_fmt && fmt != nfmt)
	{
	  if (subcmd == 'c')
	    {
	      argbuf = &cpu;
	      fmt = cpu_fmt;
	    }
	  else
	    fmt = nfmt;
	  return EXTRA_INPUT;
	}

      Thread *t = Jdb::get_current_active();
      switch (subcmd)
	{
	case 'r': cpu = 0; list_threads(t, 'r'); break;
	case 'p': list_threads(t, 'p'); break;
	case 'c': 
		  if (Cpu::online(cpu))
		    list_threads(Jdb::get_thread(cpu), 'r');
		  else
		    printf("\nCPU %u is not online!\n", cpu);
		  cpu = 0;
		  break;
	case 't': Jdb::execute_command("lt"); break; // other module
	}
    }
  else if (cmd == 1)
    {
      Console *gzip = Kconsole::console()->find_console(Console::GZIP);
      if (gzip)
	{
	  Thread *t = Jdb::get_current_active();
	  gzip->state(gzip->state() | Console::OUTENABLED);
	  long_output = 1;
	  Jdb_thread_list::init('p', t);
	  Jdb_thread_list::set_start(t);
	  Jdb_thread_list::goto_home();
	  Jdb_thread_list::complete_show(list_threads_show_thread);
	  long_output = 0;
	  gzip->state(gzip->state() & ~Console::OUTENABLED);
	}
      else
	puts(" gzip module not available");
    }

  return NOTHING;
}
#endif


PUBLIC
void
Jdb_list::show_header()
{
  Jdb::cursor();
  printf("%.*s\033[K\n", (int)Jdb_screen::width(), show_head());
}

PUBLIC
void
Jdb_list::do_list()
{
  int y, y_max;
  void *t;

  if (!_start)
    _start = get_head();

  if (!_current)
    _current = _start;

  Jdb::clear_screen();
  show_header();

  if (!_start)
    {
      printf("[EMPTY]\n");
      return;
    }


  for (;;)
    {
      _start = get_visible(_start);
      // set y to position of t_current in current displayed list
      y = lookup_in_visible_area(_current);
      if (y == -1)
	{
	  _start = _current;
	  y = 0;
	}

      for (bool resync=false; !resync;)
	{
	  Jdb::cursor(2, 1);
	  y_max = page_show();

	  // clear rest of screen (if where less than 24 lines)
	  for (unsigned i = y_max; i < Jdb_screen::height()-3; ++i)
            putstr("\033[K\n");

          char const *d = "<Space>=mode <Tab>=link <CR>=select /=filter";
          if (_filter_str[0])
            Jdb::printf_statline("Objs", d,
                                 "%s (%s)", get_mode_str(), _filter_str);
          else
            Jdb::printf_statline("Objs", d, "%s", get_mode_str());

	  // key event loop
	  for (bool redraw=false; !redraw; )
	    {
	      Jdb::cursor(y+2, 1);
	      switch (int c=Jdb_core::getchar())
		{
		case KEY_CURSOR_UP:
		case 'k':
		  if (y > 0)
		    y--;
		  else
		    redraw = line_back();
		  break;
		case KEY_CURSOR_DOWN:
		case 'j':
		  if (y < y_max)
		    y++;
		  else
		    redraw = line_forw();
		  break;
		case KEY_PAGE_UP:
		case 'K':
		  if (!(redraw = page_back()))
		    y = 0;
		  break;
		case KEY_PAGE_DOWN:
		case 'J':
		  if (!(redraw = page_forw()))
		    y = y_max;
		  break;
		case KEY_CURSOR_HOME:
		case 'H':
		  redraw = goto_home();
		  y = 0;
		  break;
		case KEY_CURSOR_END:
		case 'L':
		  redraw = goto_end();
		  y = y_max;
		  break;
		case 's': // switch sort
		  _current = index(y);
		  next_sort();
		  redraw = true;
		  resync = true;
		  break;
		case ' ': // switch mode
                  _current = index(y);
		  next_mode();
		  _current = get_valid(_current);
                  _start   = get_valid(_start);
		  redraw = true;
		  resync = true;
		  break;
                case '/':
                  handle_string_filter_input();
                  _current = get_visible(_current);
                  redraw = true;
                  resync = true;
                  break;
		case KEY_TAB: // go to associated object
		  _current = index(y);
		  t = follow_link(_current);
		  if (t != _current)
		    {
		      _current = t;
		      redraw = true;
		      resync = true;
		    }
		  break;
		case KEY_RETURN:
		case KEY_RETURN_2:
		  _current = index(y);
		  if (!enter_item(_current))
		    return;
		  show_header();
		  redraw = 1;
		  break;
		case KEY_ESC:
		  Jdb::abort_command();
		  return;
		default:
		  _current = index(y);
		  if (!handle_key(_current, c) && Jdb::is_toplevel_cmd(c))
		    return;

		  show_header();
		  redraw = 1;
		  break;
		}
	    }
	}
    }
}

