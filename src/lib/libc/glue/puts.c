#include "stdio_impl.h"
#include "fiasco_stdio.h"

int puts(const char *s)
{
  FILE f = (FILE)
    {
      .flags = F_PERM | F_NORD,
      .write = __fiasco_stdout_write,
      .buf = NULL,
      .buf_size = 0, /* vfprintf() will use local internal_buf */
      .fd = 1,
      .lock = -1,
      .lbf = '\n',
    };
  return -(fputs(s, &f) < 0 || putc_unlocked('\n', &f) < 0);
}
