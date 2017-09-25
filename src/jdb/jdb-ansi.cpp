
/*
 * JDB Module implementing ANSI/vt100 functions
 */

INTERFACE:

EXTENSION class Jdb
{
public:
  enum
  {
    NOFANCY=0,
    FANCY=1
  };

  enum Direction
  {
    Cursor_up = 'A',
    Cursor_down = 'B',
    Cursor_right = 'C',
    Cursor_left = 'D'
  };
};

IMPLEMENTATION:

#include <cstdio>
#include <simpleio.h>
#include "jdb_screen.h"

PUBLIC static inline
void
Jdb::cursor( Direction d, unsigned n = 1)
{
  printf("\033[%u%c", n, (char)d);
}


PUBLIC static
void
Jdb::cursor (unsigned int row=0, unsigned int col=0)
{
  if (row || col)
    printf ("\033[%u;%uH", row, col);
  else
    printf ("\033[%u;%uH", 1u, 1u);
}

PUBLIC static inline NEEDS[<cstdio>]
void
Jdb::blink_cursor (unsigned int row, unsigned int col)
{
  printf ("\033[%u;%uf", row, col);
}

PUBLIC static inline NEEDS[<simpleio.h>]
void
Jdb::cursor_save()
{
  putstr ("\0337");
}

PUBLIC static inline NEEDS[<simpleio.h>]
void
Jdb::cursor_restore()
{
  putstr ("\0338");
}

PUBLIC static inline NEEDS[<simpleio.h>]
void
Jdb::screen_erase()
{
  putstr ("\033[2J");
}   

PUBLIC static
void
Jdb::screen_scroll (unsigned int start, unsigned int end)
{
  if (start || end)
    printf ("\033[%u;%ur", start, end);
  else
    printf ("\033[r");
}

PUBLIC static inline NEEDS[<simpleio.h>]
void
Jdb::clear_to_eol()
{
  putstr("\033[K");
}

// preserve the history of the serial console if fancy != 0
PUBLIC static
void
Jdb::clear_screen(int fancy=FANCY)
{
  if (fancy == FANCY)
    {
      cursor(Jdb_screen::height(), 1);
      for (unsigned i=0; i<Jdb_screen::height(); i++)
	{
	  putchar('\n');
	  clear_to_eol();
	}
    }
  else
    {
      cursor();
      for (unsigned i=0; i<Jdb_screen::height()-1; i++)
	{
	  clear_to_eol();
	  putchar('\n');
	}
    }
  cursor();
}

