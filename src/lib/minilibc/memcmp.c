#include <stddef.h>

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

int bcmp(const char *a,const char *b,size_t c)	__attribute__((weak,alias("memcmp")));
