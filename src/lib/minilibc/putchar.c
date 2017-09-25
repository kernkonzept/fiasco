#include <stdio.h>
#include "libc_backend.h"

int putchar (int c) 
{
  char _c = c;
  unsigned long state = __libc_backend_printf_lock();
  int ret = __libc_backend_outs(&_c,1) ? c : 0;
  __libc_backend_printf_unlock(state);
  return ret;
}
