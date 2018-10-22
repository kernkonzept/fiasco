#include <string.h>

char* stpcpy(char *d, const char *s)
{
  while ((*d++ = *s++))
    ;
  return d - 1;
}
