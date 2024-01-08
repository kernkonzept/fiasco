#define _POSIX_SOURCE
#define _XOPEN_SOURCE
#include <string.h>

char *strncpy(char *dest, const char *src, size_t n)
{
  // In contrast to POSIX strncpy(), don't zero-pad the remaining target buffer.
  memccpy(dest,src,0,n);
  return dest;
}
