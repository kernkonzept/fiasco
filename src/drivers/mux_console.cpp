INTERFACE:

#include "types.h"
#include "console.h"

/**
 * Console multiplexer.
 *
 * This implementation of the Console interface can be used to
 * multiplex among some input, output, and in-out consoles.
 */
class Mux_console : public Console
{
public:

  enum
  {
    SIZE = 8  ///< The maximum number of consoles to be multiplexed.
  };

  int  write(char const *str, size_t len);
  int  getchar(bool blocking = true);
  int  char_avail() const;

private:
  int     _next_getchar;
  int     _items;
  Console *_cons[SIZE];
  Unsigned64 _ignore_input_until;
};


IMPLEMENTATION:

#include <cstdio>
#include "kip.h"
#include "processor.h"
#include "string.h"

PUBLIC
Mux_console::Mux_console()
: Console(ENABLED), _next_getchar(-1), _items(0)
{}

IMPLEMENT
int
Mux_console::write(char const *str, size_t len)
{
  for (int i = 0; i < _items; ++i)
    if (_cons[i] && (_cons[i]->state() & OUTENABLED))
      _cons[i]->write(str, len);

  return len;
}

PUBLIC
void
Mux_console::set_ignore_input()
{
  _ignore_input_until = ~0ULL;
}

PUBLIC
void
Mux_console::set_ignore_input(Unsigned64 delta)
{
  _ignore_input_until = Kip::k()->clock + delta;
}

PRIVATE
int
Mux_console::check_input_ignore()
{
  if (_ignore_input_until)
    {
      if (Kip::k()->clock > _ignore_input_until)
        _ignore_input_until = 0;
      else
        {
          static unsigned releasepos;
          const char *releasestring = "input";

          // when we ignore input we read everything which comes in even if
          // input on a console is disabled
          for (int i = 0; i < _items; ++i)
            if (_cons[i])
              {
                int r;
                while ((r = _cons[i]->getchar(false)) != -1)
                  {
                    if (releasestring[releasepos] == r)
                      {
                        releasepos++;
                        if (releasepos == strlen(releasestring))
                          {
                            printf("\nJDB: Input activated.\n");
                            _ignore_input_until = 0;
                            releasepos = 0;
                            return 0;
                          }
                      }
                    else
                      releasepos = 0;
                  }
              }
          return 1;
        }
    }

  return 0;
}

IMPLEMENT
int
Mux_console::getchar(bool blocking)
{
  if (check_input_ignore())
    return -1;

  if (_next_getchar != -1)
    {
      int c = _next_getchar;
      _next_getchar = -1;
      return c;
    }

  int ret = -1;
  do
    {
      int conscnt = 0;
      for (int i = 0; i < _items; ++i)
        if (_cons[i] && (_cons[i]->state() & INENABLED))
          {
            ret = _cons[i]->getchar(false);
            if (ret != -1)
              return ret;
            ++conscnt;
          }

      if (!conscnt)
        break;

      if (blocking)
	Proc::pause();
    }
  while (blocking && ret == -1);

  return ret;
}

/**
 * deliver attributes of all subconsoles.
 */
PUBLIC
Mword
Mux_console::get_attributes() const
{
  Mword attr = 0;

  for (int i = 0; i < _items; i++)
    if (_cons[i])
      attr |= _cons[i]->get_attributes();

  return attr;
}

PUBLIC
void
Mux_console::getchar_chance()
{
  for (int i = 0; i < _items; ++i)
    if (   _cons[i] && (_cons[i]->state() & INENABLED)
        && _cons[i]->char_avail() == 1)
      {
        int c = _cons[i]->getchar(false);
        if (c != -1 && _next_getchar == -1)
          _next_getchar = c;
      }
}

IMPLEMENT
int
Mux_console::char_avail() const
{
  int ret = -1;
  for (int i = 0; i < _items; ++i)
    if (_cons[i] && (_cons[i]->state() & INENABLED))
      {
        int tmp = _cons[i]->char_avail();
        if (tmp == 1)
          return 1;
        else if (tmp == 0)
          ret = tmp;
      }
  return ret;
}

/**
 * Register a console to be multiplexed.
 * @param cons the Console to add.
 * @param pos the position of the console, normally not needed.
 */
PUBLIC virtual
bool
Mux_console::register_console(Console *c, int pos = 0)
{
  if (c->failed())
    return false;

  if (_items >= SIZE)
    return false;

  if (pos >= SIZE || pos < 0)
    return false;

  if (pos > _items)
    pos = _items;

  if (pos < _items)
    for (int i = _items - 1; i >= pos; --i)
      _cons[i + 1] = _cons[i];

  _items++;
  _cons[pos] = c;

  return true;
}

/**
 * Unregister a console from the multiplexer.
 * @param cons the console to remove.
 */
PUBLIC
bool
Mux_console::unregister_console(Console *c)
{
  int pos;
  for (pos = 0; pos < _items && _cons[pos] != c; pos++)
    ;
  if (pos == _items)
    return false;

  --_items;
  for (int i = pos; i < _items; ++i)
    _cons[i] = _cons[i + 1];

  return true;
}

/**
 * Change the state of a group of consoles specified by
 *        attributes.
 * @param any_true   match if console has any of these attributes
 * @param all_false  match if console doesn't have any of these attributes
 */
PUBLIC
void
Mux_console::change_state(Mword any_true, Mword all_false,
			  Mword mask, Mword bits)
{
  for (int i=0; i<_items; i++)
    {
      if (_cons[i])
	{
	  Mword attr = _cons[i]->get_attributes();
	  if (   // any bit of the any_true attributes must be set
	         (!any_true  || (attr & any_true)  != 0)
	         // all bits of the all_false attributes must be cleared
	      && (!all_false || (attr & all_false) == 0))
	    {
	      _cons[i]->state((_cons[i]->state() & mask) | bits);
	    }
	}
    }
}

/**
 * Find a console with a specific attribute.
 * @param any_true match to console which has set any bit of this bitmask
 */
PUBLIC
Console*
Mux_console::find_console(Mword any_true)
{
  for (int i = 0; i < _items; i++)
    if (_cons[i] && _cons[i]->get_attributes() & any_true)
      return _cons[i];

  return 0;
}

/**
 * Start exclusive mode for a specific console. Only the one
 *        console which matches to any_true is enabled for input and
 *        output. All other consoles are disabled.
 * @param any_true match to console which has set any bit of this bitmask
 */
PUBLIC
void
Mux_console::start_exclusive(Mword any_true)
{
  // enable exclusive console
  change_state(any_true, 0, ~0UL, (OUTENABLED|INENABLED));
  // disable all other consoles
  change_state(0, any_true, ~(OUTENABLED|INENABLED), 0);
}

/**
 * End exclusive mode for a specific console.
 * @param any_true match to console which has set any bit of this bitmask
 */
PUBLIC
void
Mux_console::end_exclusive(Mword any_true)
{
  // disable exclusive console
  change_state(any_true, 0, ~(OUTENABLED|INENABLED), 0);
  // enable all other consoles
  change_state(0, any_true, ~0UL, (OUTENABLED|INENABLED));
}


IMPLEMENTATION[debug]:

PUBLIC
void
Mux_console::list_consoles()
{
  for (int i = 0; i < _items; i++)
    if (_cons[i])
      {
        Mword attr = _cons[i]->get_attributes();

        printf("  " L4_PTR_FMT "  %s  (%s)  ",
               attr, _cons[i]->str_mode(), _cons[i]->str_state());
        for (unsigned bit = 2; bit < sizeof(attr) * 4; bit++)
          if (attr & (1 << bit))
            printf("%s ", Console::str_attr(bit));
        putchar('\n');
      }
}
