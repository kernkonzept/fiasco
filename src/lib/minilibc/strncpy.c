#define _POSIX_SOURCE
#define _XOPEN_SOURCE
#include <string.h>

char *strncpy(char *dest, const char *src, size_t n)
{
  memccpy(dest,src,0,n);
  return dest;
}
