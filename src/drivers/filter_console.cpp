INTERFACE:

#include "console.h"
#include "types.h"

class Filter_console : public Console
{
  friend struct Console_test;

public:
  ~Filter_console() {}

private:
  Console *const _o;
  /**
   * The number of loops for waiting to decide if a received escape character is
   * the beginning of a CSI sequence or just a single escape key. The waiting
   * time is defined by `csi_timeout_loops` times `csi_timeout_us`.
   */
  unsigned csi_timeout_loops;
  /**
   * The amount of time to wait during each CSI timeout loop.
   */
  static constexpr unsigned csi_timeout_us = 25;

  enum State
  {
    NORMAL,
    UNKNOWN_ESC,
    GOT_CSI, ///< control sequence introducer
  };

  State state;
  unsigned pos;
  char ibuf[32];
  unsigned arg;
  int args[4];
};


IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include <cctype>
#include "keycodes.h"

PUBLIC inline explicit
Filter_console::Filter_console(Console *o, unsigned loops = 400)
: Console(ENABLED), _o(o), csi_timeout_loops(loops), state(NORMAL), pos(0), arg(0)
{
  if (o->failed())
    fail();
}


PUBLIC
int
Filter_console::write(char const *str, size_t len) override
{
  if (!(_o->state() & OUTENABLED))
    return len;

  return _o->write(str, len);
}

PUBLIC
Mword
Filter_console::get_attributes() const override
{
  return _o->get_attributes();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [input]:

#include "delayloop.h"

PUBLIC
int Filter_console::char_avail() const override
{
  if (!(_o->state() & INENABLED))
    return -1;

  if (pos)
    return 1;

  return _o->char_avail();
}


PRIVATE inline
int
Filter_console::getchar_timeout(unsigned timeout)
{
  if (!(_o->state() & INENABLED))
    return -1;

  int c;
  while ((c = _o->getchar(false)) == -1 && timeout--)
    Delay::udelay(csi_timeout_us);
  return c;
}

/**
 * Read from _o console and detect escape sequences.
 *
 * For certain escape sequences like key up/down etc, return dedicated values.
 * Other escape sequences like "\033[y;xR" are buffered and are then returned
 * one-by-one during future calls of this function.
 */
PUBLIC
int
Filter_console::getchar(bool blocking = true) override
{
  if (!(_o->state() & INENABLED))
    return -1;

  unsigned loop_count = 100;
  int ch;

  do
    {
      if (state == UNKNOWN_ESC && pos)
        {
          ch = ibuf[0];
          memmove(ibuf, ibuf + 1, --pos);
          // on pos == 0 we return 'ch' using the NORMAL case below
        }
      else if (state == GOT_CSI)
        // Be more patient while in the middle of a CSI escape sequence.
        ch = getchar_timeout(csi_timeout_loops);
      else
        ch = _o->getchar(blocking);

      if (!pos)
        state = NORMAL;

      if (ch == -1)
        {
          if (state == NORMAL || blocking)
            return -1;
          else
            continue;
        }

      switch (state)
        {
        case UNKNOWN_ESC:
          return ch;

        case NORMAL:
          if (ch == 27)
            {
              ibuf[pos++] = 27;
              int nc;
              if (!(_o->get_attributes() & Console::UART)
                  || ((nc = getchar_timeout(csi_timeout_loops)) == -1))
                {
                  pos = 0;
                  return KEY_SINGLE_ESC;
                }
              else
                {
                  /* Detect ANSI escape sequences from UART console. */
                  if (pos < sizeof(ibuf))
                    ibuf[pos++] = nc;
                  if (nc == '[' || nc == 'O')
                    {
                      arg = 0;
                      memset(args, 0, sizeof(args));
                      state = GOT_CSI;
                      break;
                    }
                  else
                    {
                      state = UNKNOWN_ESC;
                      loop_count++;
                      continue;
                    }
                }
            }
          return ch;

        case GOT_CSI:
          if (isdigit(ch))
            {
              if (pos < sizeof(ibuf))
                ibuf[pos++] = ch;

              if (arg < (sizeof(args) / sizeof(int)))
                args[arg] = args[arg] * 10 + (ch - '0');
            }
          else if (ch == ';')
            {
              if (pos < sizeof(ibuf))
                ibuf[pos++] = ch;

              arg++;
            }
          else
            {
              state = NORMAL;
              if (pos < sizeof(ibuf))
                ibuf[pos++] = ch;

              switch(ch)
                {
                case 'A': pos = 0; return KEY_CURSOR_UP;
                case 'B': pos = 0; return KEY_CURSOR_DOWN;
                case 'C': pos = 0; return KEY_CURSOR_RIGHT;
                case 'D': pos = 0; return KEY_CURSOR_LEFT;
                case 'H': pos = 0; return KEY_CURSOR_HOME;
                case 'F': pos = 0; return KEY_CURSOR_END;
                case '~':
                  pos = 0;
                  switch (args[0])
                    {
                    case  7:
                    case  1: return KEY_CURSOR_HOME;
                    case  2: return KEY_INSERT;
                    case  3: return KEY_DELETE;
                    case  8:
                    case  4: return KEY_CURSOR_END;
                    case  5: return KEY_PAGE_UP;
                    case  6: return KEY_PAGE_DOWN;
                    case 11: return KEY_F1;

                    default:
                      arg = 0;
                      if (blocking)
                        loop_count++;
                      continue;
                    }
                case 'P': return KEY_F1;
                default:
                  state = UNKNOWN_ESC;
                  break;
                }
            }
          break;
        }

      if (blocking)
        loop_count++;
    }
  while (loop_count--);

  return -1;
}

