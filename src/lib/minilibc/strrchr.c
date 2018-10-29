#include <string.h>

char *strrchr(register const char *t, int c)
{
  char ch;
  register const char *t1 = t;

  ch = c;
  if (!*t1)
    return NULL;
  while (*(t+1) != '\0')
    ++t1;
  for (;;)
    {
      if (*t1 == ch)
        break;
      if (t1 == t)
        return 0;
      --t1;
    }
  return (char*)t1;
}
