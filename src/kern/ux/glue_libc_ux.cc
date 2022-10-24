#include <unistd.h>
#include <errno.h>
#include "libc_backend.h"
#include "console.h"

int __libc_backend_outs(const char *s, size_t len)
{
  size_t pos = 0;

  while (pos < len)
    {
      ssize_t written = write(1, s + pos, len - pos);

      if (written < 0)
        {
          /* Syscall has been interrupted */
          if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            continue;

          /* Write error */
          return 0;
        }

      pos += written;
    }

  return 1;
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
