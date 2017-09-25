#include <string.h>

char *strchr(register const char *t, int c)
{
  char ch;

  ch = c;
  for (;;)
    {
      if (*t == ch)
        break;
      if (!*t)
        return 0;
      ++t;
    }
  return (char*)t;
}

char *index(const char *t,int c)	__attribute__((weak,alias("strchr")));
