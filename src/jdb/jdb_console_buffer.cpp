IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "config.h"
#include "jdb_module.h"
#include "jdb.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "kmem_alloc.h"
#include "koptions.h"
#include "static_init.h"

/**
 * A output console that stores the output in a buffer.
 *
 * This buffer can be useful for accessing older the debugging
 * output without a serial console.
 */
class Console_buffer : public Console
{
private:
  static bool _enabled;
  static size_t out_buf_size;
  static size_t out_buf_len;
  static char *out_buf;
  static char *out_buf_w;
};

bool   Console_buffer::_enabled;
size_t Console_buffer::out_buf_size;
size_t Console_buffer::out_buf_len;
char*  Console_buffer::out_buf;
char*  Console_buffer::out_buf_w;

PRIVATE static
void
Console_buffer::at_jdb_enter()
{
  _enabled = false;
}

PRIVATE static
void
Console_buffer::at_jdb_leave()
{
  _enabled = true;
}

PUBLIC
Console_buffer::Console_buffer() : Console(ENABLED)
{
  static Jdb_handler enter(at_jdb_enter);
  static Jdb_handler leave(at_jdb_leave);

  Jdb::jdb_enter.add(&enter);
  Jdb::jdb_leave.add(&leave);

  size_t len = 2 * Config::PAGE_SIZE;

  if (Koptions::o()->opt(Koptions::F_out_buf))
    len = Koptions::o()->out_buf;

  alloc(len);
}

/**
 * Allocates a buffer of the given size.
 * @param size the buffer size in bytes.
 */
PUBLIC static
void
Console_buffer::alloc(size_t size)
{
  if (!out_buf)
    {
      out_buf_size = (size + Config::PAGE_SIZE - 1) & Config::PAGE_MASK;
      if (out_buf_size)
	out_buf = (char *)Kmem_alloc::allocator()->
                            unaligned_alloc(out_buf_size);

      out_buf_w = out_buf;

      _enabled = true;
    }
}

PUBLIC
Console_buffer::~Console_buffer()
{
  if(out_buf)
    Kmem_alloc::allocator()->
      unaligned_free(out_buf_size, out_buf);
  out_buf = 0;
}

PUBLIC
int
Console_buffer::write( char const *str, size_t len )
{
  if (_enabled && out_buf)
    {
      while(len)
	{
	  size_t s;
	  s = out_buf_size - (out_buf_w - out_buf);
	  if (s>len)
	    s = len;
	  memcpy( out_buf_w, str, s );
	  if (out_buf_w + s >= out_buf + out_buf_size)
	    out_buf_w = out_buf;
	  else
	    out_buf_w += s;
	  len -= s;
	  out_buf_len += s;
	  if (out_buf_len > out_buf_size)
	    out_buf_len = out_buf_size;
	}
    }
  return len;
}

PRIVATE static inline
void
Console_buffer::inc_ptr(char **c)
{
  if (++*c >= out_buf + out_buf_size)
    *c = out_buf;
}

PRIVATE static inline
void
Console_buffer::dec_out_ptr(char **c)
{
  if (--*c < out_buf)
    *c += out_buf_size;
}

PUBLIC
int
Console_buffer::getchar(bool)
{
  return -1;
}

/**
 * Prints the buffer to the standard I/O.
 * @param lines the number of lines to skip.
 * This method prints the buffer contents to the normal I/O.
 * Before doing this the buffer is disabled, that no recursion 
 * appers even if the buffer is part of the muxed I/O.
 */
PUBLIC static
int
Console_buffer::print_buffer(unsigned lines)
{
  if (out_buf)
    {
      bool page  = (lines == 0);
      char   *c  = out_buf_w;
      size_t len = out_buf_len;

      if (out_buf_len == 0)
	{
	  puts("<empty>\n");
	  return 1;
	}

      // go back <lines> lines
      if (lines)
	{
	  size_t l = out_buf_len;

	  // skip terminating 0x00, 0x0a ...
	  while ((*c == '\0' || *c == '\r') && (l > 0))
	    {
	      dec_out_ptr(&c);
	      l--;
	    }

	  while (lines-- && (l > 0))
	    {
	      dec_out_ptr(&c);
	      l--;

	      while ((*c != '\n') && (l > 0))
		{
		  dec_out_ptr(&c);
		  l--;
		}
	    }

	  if (*c == '\n')
	    {
	      inc_ptr(&c);
	      l++;
	    }

	  len = out_buf_len - l;
	}
      else
	{
	  c -= out_buf_len;
	  if (c < out_buf)
	    c += out_buf_size;
	}

      lines = 0;
      while (len > 0)
	{
	  putchar(*c);
	  if (*c == '\n')
	    {
	      if (page && !Jdb_core::new_line(lines))
		return 1;
	    }
	  inc_ptr(&c);
	  len--;
	}

      putchar('\n');
      return 1;
    }

  printf("use -out_buf=<size> to enable output buffer\n");
  return 0;
}


PRIVATE static
int
Console_buffer::strncmp(char *start, const char *search, size_t len)
{
  while (len && *search && *start == *search)
    {
      len--;
      inc_ptr(&start);
      search++;
    }

  return *search == '\0';
}

/**
 * Prints the buffer to the standard I/O.
 * @param str the string the output should be filtered for
 * This method prints the buffer contents to the normal I/O.
 * Before doing this the buffer is disabled, that no recursion
 * appers even if the buffer is part of the muxed I/O.
 */
PUBLIC static
int
Console_buffer::print_buffer(const char *str)
{
  if (out_buf)
    {
      char   *bol = out_buf_w;
      size_t len  = out_buf_len;

      if (out_buf_len == 0)
	{
	  puts("<empty>\n");
	  return 1;
	}

      bol -= out_buf_len;
      if (bol < out_buf)
	bol += out_buf_size;

next_line:
      while (len > 0)
	{
	  char   *bos = bol;
	  size_t lens = len;
	  const char *s;

	  while (lens > 0)
	    {
	      if (*bos == '\n')
		{
		  bol = bos;
		  inc_ptr(&bol);
		  len = lens-1;
		  goto next_line;
		}
	      if (strncmp(bos, str, lens))
		{
		  // found => print whole line
		  for (; len && bol != bos; len--)
		    {
		      putchar(*bol);
		      inc_ptr(&bol);
		    }
	    found_again:
		  putstr(Jdb::esc_emph);
		  for (s=str; *s && len; len--, s++)
		    {
		      putchar(*bol);
		      inc_ptr(&bol);
		    }
		  putstr("\033[m");
		  for (; len && *bol != '\n'; len--)
		    {
		      if (*str && strncmp(bol, str, len))
			goto found_again;
		      putchar(*bol);
		      inc_ptr(&bol);
		    }
		  if (len)
		    {
		      putchar(*bol);
		      inc_ptr(&bol);
		      len--;
		    }
		  goto next_line;
		}
	      inc_ptr(&bos);
	      lens--;
	    }
	}

      putchar('\n');
      return 1;
    }

  printf("use -out_buf=<size> to enable output buffer\n");
  return 0;
}

PUBLIC
Mword Console_buffer::get_attributes() const
{
  return BUFFER | OUT;
}


PUBLIC static FIASCO_INIT
void
Console_buffer::init()
{
  static Console_buffer cb;
  Kconsole::console()->register_console(&cb);
}

STATIC_INITIALIZE(Console_buffer);



/// Jdb module

class Jdb_cb : public Jdb_module
{
public:
  Jdb_cb() FIASCO_INIT;
private:
  static char  first_char;
  static char  search_str[30];
  static Mword output_lines;
};

char  Jdb_cb::first_char;
char  Jdb_cb::search_str[30];
Mword Jdb_cb::output_lines;

PUBLIC
Jdb_module::Action_code
Jdb_cb::action(int cmd, void *&args, char const *&fmt, int &next_char)
{
  if (cmd != 0)
    return NOTHING;

  if (args == &first_char)
    {
      if (first_char == '/')
	{
	  putchar(first_char);
	  fmt  = "%30s";
	  args = search_str;
	  return EXTRA_INPUT;
	}
      output_lines = 0;
      if (first_char != ' ' && first_char != KEY_RETURN
          && first_char != KEY_RETURN_2)
	{
	  next_char = first_char;
	  args = &output_lines;
	  fmt  = "%4d";
	  return EXTRA_INPUT_WITH_NEXTCHAR;
	}
    }
  else if (args == search_str)
    {
      putchar('\n');
      Console_buffer::print_buffer(search_str);
      return NOTHING;
    }

  putchar('\n');
  Console_buffer::print_buffer(output_lines);

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_cb::cmds() const
{
  static const Cmd cs[] =
    {
	{ 0, "B", "consolebuffer", "%C",
	  "B[<lines>|/<str>]\tshow (last n lines of) console buffer/search",
	  &first_char },
    };
  return cs;
}

PUBLIC
int
Jdb_cb::num_cmds() const
{
  return 1;
}

IMPLEMENT
Jdb_cb::Jdb_cb()
  : Jdb_module("GENERAL")
{
}

static Jdb_cb jdb_cb INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
