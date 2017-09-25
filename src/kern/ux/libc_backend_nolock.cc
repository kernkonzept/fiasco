#include <libc_backend.h>

void __libc_backend_printf_local_force_unlock()
{
}

unsigned long __libc_backend_printf_lock()
{
  return 1;
}

void __libc_backend_printf_unlock(unsigned long)
{
}
