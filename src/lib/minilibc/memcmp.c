#include <stddef.h>
#include <string.h>

int memcmp(const void *dst, const void *src, size_t count) {
  register int r;
  register const char *d=dst;
  register const char *s=src;
  while (count--) {
    if ((r=(*d - *s)))
      return r;
    ++d;
    ++s;
  }
  return 0;
}
