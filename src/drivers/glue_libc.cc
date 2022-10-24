#include "libc_backend.h"
#include "console.h"

int __libc_backend_outs(const char *str, size_t len)
{
  if (!Console::stdout)
    return 1;

  char const *end = str + len;
  char const *delim = str;

  while (str < end)
    {
      // Find the delimiter in the current part of the string which points
      // either to the next newline character or to the end of the string.
      while (delim < end && *delim != '\n')
        ++delim;

      // Output the current part up to (but not including) the delimiter.
      while (str < delim)
        {
          int written = Console::stdout->write(str, delim - str);
          if (written < 0)
            return 0;

          str += written;
        }

      // If the delimiter is a newline, then output CR+LF.
      if (delim < end && *delim == '\n')
        {
          int written = Console::stdout->write("\r\n", 2);
          if (written < 0)
            return 0;

          ++delim;
          ++str;
        }
    }

  return 1;
}

int __libc_backend_ins(char *s, size_t len)
{
  if (Console::stdin)
    {
      size_t act = 0;
      for (; act < len; act++)
        {
          s[act] = Console::stdin->getchar();
          if (s[act] == '\r')
            {
              act++;
              break;
            }
        }
      return act;
    }

  return 0;
}
