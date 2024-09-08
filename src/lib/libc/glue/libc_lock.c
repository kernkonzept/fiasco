#include "lock.h"
#include "libc_backend.h"

void __lock(volatile int *v)
{
  *v = __libc_backend_printf_lock();
}

void __unlock(volatile int *v)
{
  __libc_backend_printf_unlock(*v);
}
