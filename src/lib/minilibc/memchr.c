#include <string.h>

void* memchr(const void *s, int c, size_t n) {
  register const char* t=s;
  int i;
  for (i=n; i; --i) {
    if (*t==c)
      return (char*)t;
    ++t;
  }
  return 0;
}
