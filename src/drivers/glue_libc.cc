#include "libc_backend.h"
#include "console.h"

int __libc_backend_outs(const char *s, size_t len)
{
  if (!Console::stdout)
    return len;

  char const *e = s + len;
  char const *p = s;
  while (s < e)
    {
      for (; p < e && *p != '\n'; ++p)
        ;

      while (s < p)
        {
          int written = Console::stdout->write(s, p - s);
          if (written < 0)
            return written;
          s += written;
        }

      if (p < e && *p == '\n')
        {
          Console::stdout->write("\r", 1);
          ++p;
        }
    }

  return len;
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
