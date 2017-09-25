#include <unistd.h>
#include "libc_backend.h"
#include "console.h"

int __libc_backend_outs(const char *s, size_t len)
{
  return write(1, s, len);
}

int __libc_backend_ins(char *s, size_t len)
{
  if (!Console::stdin)
    return 0;

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
