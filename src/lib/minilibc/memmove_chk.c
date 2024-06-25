#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <stddef.h>
#include <string.h>
#include <panic.h>

void *__memmove_chk(void *dst, const void *src, size_t count, size_t destlen);
void *__memmove_chk(void *dst, const void *src, size_t count, size_t destlen)
{
  if (count > destlen)
    panic("memmove buffer overflow");

  return memmove(dst, src, count);
}
