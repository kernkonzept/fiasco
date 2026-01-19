#include <string.h>
#include <stdint.h>
#include <libc_backend.h>
#include "types.h"

void *memcpy(void *dest, const void *src, size_t n)
{
  unsigned char *d = static_cast<unsigned char *>(dest);
  const unsigned char *s = static_cast<unsigned char const *>(src);
  for (; n; n--)
    *d++ = *s++;
  return dest;
}

unsigned int __libc_backend_printf_lock()
{ return 0; }

void __libc_backend_printf_unlock(unsigned int)
{}

// Output to pl011!
int __libc_backend_outs(const char *s, size_t len)
{
  Unsigned32 *pl011 = reinterpret_cast<Unsigned32 *>(0x9000000);
  size_t i = 0;
  for (; i < len; ++i)
    {
      while (access_once(&pl011[0x18 / 4]) & 0x20)
        ;
      write_now(&pl011[0x00], s[i]);
    }
  return 1;
}
