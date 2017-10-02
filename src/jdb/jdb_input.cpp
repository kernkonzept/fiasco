INTERFACE:

#include "types.h"
#include "l4_types.h"

class Jdb_input
{
};

IMPLEMENTATION:

#include <cctype>
#include <cstring>
#include <cstdio>
#include "keycodes.h"
#include "jdb.h"
#include "jdb_symbol.h"
#include "simpleio.h"

PUBLIC static
int
Jdb_input::get_mword(Mword *mword, int digits, int base, int first_char = 0)
{
  Mword val = 0;

  for(int digit = 0; digit < digits; )
    {
      int c, v;

      if (first_char)
        {
          c = v = first_char;
          first_char = 0;
        }
      else
        c = v = Jdb_core::getchar();
      switch (c)
        {
        case 'a': case 'b': case 'c':
        case 'd': case 'e': case 'f':
          if (base == 16)
            v -= 'a' - '9' - 1;
          else
            continue;
          // FALLTHRU
        case '0': case '1': case '2':
        case '3': case '4': case '5':
        case '6': case '7': case '8':
        case '9':
          val = (val * base) + (v - '0');
          digit++;
          putchar(c);
          break;
        case KEY_BACKSPACE:
          if (digit)
            {
              putstr("\b \b");
              digit--;
              val /= base;
            }
          break;
        case 'x':
          // If last digit was 0, delete it. This makes it easier to cut
          // 'n paste hex values like 0x12345678 into the serial terminal
          if (base == 16 && digit && ((val & 0x10) == 0))
            {
              putstr("\b \b");
              digit--;
              val /= base;
            }
          break;
        case ' ':
        case KEY_RETURN:
        case KEY_RETURN_2:
          if (digit)
            {
              *mword = val;
              return true;
            }
          *mword = 0;
          return false;
        case KEY_ESC:
          return 0;
        }
    }
  *mword = val;
  return 1;
}

PUBLIC static
int
Jdb_input::get_string(char *string, unsigned size)
{
  for (unsigned pos = strlen(string); ; )
    switch (int c = Jdb_core::getchar())
      {
      case KEY_BACKSPACE:
        if (pos)
          {
            putstr("\b \b");
            string[--pos] = '\0';
          }
        break;
      case KEY_RETURN:
        return 1;
      case KEY_ESC:
        Jdb::abort_command();
        return 0;
      default:
        if (c >= ' ' && pos < size - 1)
          {
            putchar(c);
            string[pos++] = c;
            string[pos  ] = '\0';
          }
      }
}
