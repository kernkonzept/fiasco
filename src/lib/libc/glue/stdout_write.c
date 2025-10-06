#include "libc_stdio.h"

#include "libc_backend.h"

void __libc_stdout_write(FILE *f, const char *buf, size_t len)
{
  __libc_backend_outs(f->buf, f->wpos - f->buf);
  __libc_backend_outs(buf, len);
  f->wpos = f->buf;
}
