
#include <cstdio>

int
putstr(const char *const s)
{
  return printf("%s", s);
}

int
putnstr(const char *const s, int len)
{
  // the printf version in minilibc just doesn't seem to understand the
  // printf below (doesn't cut the string after len but prints all of the
  // string), so do it in another way
#if 1
  char *a = (char *)s;
  int l = 0;
  while (*a && len)
    {
      putchar(*a);
      l++;
      len--;
      a++;
    }
  return l;
#else
  printf("%*.*s", len, len, s);
#endif
}
