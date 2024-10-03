#include <stdio.h>
#include <stdarg.h>
#include "stdio_impl.h"
#include "libc_stdio.h"

int printf(const char *restrict fmt, ...)
{
  int r;
  va_list ap;
  FILE f = (FILE)
    {
      .flags = F_PERM | F_NORD,
      .write = __libc_stdout_write,
      .buf = NULL,
      .buf_size = 0, /* vfprintf() will use local internal_buf */
      .lock = -1,
      .lbf = -1,
    };
  va_start(ap, fmt);
  r = vfprintf(&f, fmt, ap);
  va_end(ap);
  return r;
}
