#include <stdio.h>
#include "stdio_impl.h"
#include "fiasco_stdio.h"

int vprintf(const char *restrict fmt, va_list ap)
{
  FILE f = (FILE)
    {
      .flags = F_PERM | F_NORD,
      .write = __fiasco_stdout_write,
      .buf = NULL,
      .buf_size = 0, /* vfprintf() will use local internal_buf */
      .fd = 1,
      .lock = -1,
      .lbf = -1,
    };
  return vfprintf(&f, fmt, ap);
}
