#include <string.h>

size_t
strlen(const char *s)
{
  register int i;
  for (i = 0; *s; ++s)
    ++i;
  return i;
}
