#include <libc_backend.h>

#include "boot_direct_cons.h"

unsigned long __libc_backend_printf_lock()
{ return 0; }

void __libc_backend_printf_unlock(unsigned long)
{}

int __libc_backend_outs(const char *s, size_t len)
{
  size_t i = 0;
  for (; i < len; ++i)
    direct_cons_putchar(s[i]);
  return 1;
}
