#include <stddef.h>

void* memcpy(void* dst, const void* src, size_t count) {
  register char *d=dst;
  register const char *s=src;
  ++count;	/* this actually produces better code than using count-- */
  while (--count) {
    *d = *s;
    ++d; ++s;
  }
  return dst;
}
