#include <stdio.h>
#include "stdio_impl.h"
#include "libc_stdio.h"

int vprintf(const char *restrict fmt, va_list ap)
{
  FILE f = (FILE)
    {
      .write = __libc_stdout_write,
      .buf = NULL,
      .buf_size = 0, /* vfprintf() will use local internal_buf */
      .lock = -1,
      .lbf = -1,
    };
  return vfprintf(&f, fmt, ap);
}
