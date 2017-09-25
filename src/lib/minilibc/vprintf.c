#include <stdarg.h>
#include <string.h>

#include "vprintf_backend.h"
#include "libc_backend.h"

int vprintf(const char *format, va_list ap)
{
  struct output_op _ap = { 0, (output_func*) &__libc_backend_outs };
  unsigned long state = __libc_backend_printf_lock();
  int ret = __v_printf(&_ap,format,ap);
  __libc_backend_printf_unlock(state);

  return ret;
}
