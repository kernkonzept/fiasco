#include "stdio_impl.h"
#include "libc_stdio.h"

int puts(const char *s)
{
  FILE f = (FILE)
    {
      .flags = F_PERM | F_NORD,
      .write = __libc_stdout_write,
      .buf = NULL,
      .buf_size = 0, /* vfprintf() will use local internal_buf */
      .lock = -1,
      .lbf = '\n',
    };
  return -(fputs(s, &f) < 0 || putc_unlocked('\n', &f) < 0);
}
