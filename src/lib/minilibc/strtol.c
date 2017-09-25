#include <ctype.h>
#include <stdlib.h>

#define ABS_LONG_MIN 2147483648UL

long int strtol (const char *nptr, char **endptr, int base) {

  int neg = 0;
  unsigned long int v;

  while (isspace (*nptr))
    nptr++;

  if (*nptr == '-') {
    neg = -1;
    ++nptr;
  }
  
  v = strtoul (nptr, endptr, base);
  
  if (v >= ABS_LONG_MIN) {
    if (v == ABS_LONG_MIN && neg) {
      return v;
    }
    return 0; //(neg ? LONG_MIN : LONG_MAX);
  }
  
  return (neg ? -v : v);
}
