#include <string.h>
#include <stdlib.h>
#include <div32.h>
#include <mod32.h>

int __lltostr(char *s, int size, unsigned long long i, int base, int UpCase)
{
  char *tmp;
  unsigned int j = 0;

  s[--size] = 0;

  tmp = s + size;
  if (base == 0 || base > 36)
    base = 10;

  j = 0;
  if (!i)
    {
      *(--tmp) = '0';
      j = 1;
    }

  while (tmp > s && i)
    {
      tmp--;
      if ((*tmp = mod32(i, base) + '0') > '9')
        *tmp += (UpCase ? 'A' : 'a') - '9' - 1;
      i = div32(i, base);
      j++;
    }
  memmove(s, tmp, j + 1);

  return j;
}
