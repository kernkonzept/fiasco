#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <stddef.h>
#include <string.h>

void *memccpy(void *dst, const void *src, int c, size_t count)
{
  char *a = dst;
  const char *b = src;
  while (count--)
  {
    *a++ = *b;
    if (*b==c)
    {
      return (void *)a;
    }
    b++;
  }
  return 0;
}
