#include <stddef.h>
#include <string.h>

int strncmp(const char *s1, const char *s2, size_t n) {
  register const char* a=s1;
  register const char* b=s2;
  register const char* fini=a+n;
  while (a<fini) {
    register int res=*a-*b;
    if (res) return res;
    if (!*a) return 0;
    ++a; ++b;
  }
  return 0;
}
