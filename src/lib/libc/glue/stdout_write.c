#include "libc_stdio.h"

#include "libc_backend.h"

size_t __libc_stdout_write(FILE *f, const unsigned char *buf, size_t len)
{
  __libc_backend_outs((const char *)f->wbase, f->wpos - f->wbase);
  __libc_backend_outs((const char *)buf, len);
  f->wend = f->buf + f->buf_size;
  f->wpos = f->wbase = f->buf;
  return len;
}
