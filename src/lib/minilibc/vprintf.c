#include <stdarg.h>
#include <string.h>

#include "vprintf_backend.h"
#include "libc_backend.h"

static int __libc_backend_outs_wrapper(char const *s, size_t len, void *ignore)
{
  (void)ignore;
  return __libc_backend_outs(s, len);
}

int vprintf(const char *format, va_list ap)
{
  struct output_op _ap = { 0, &__libc_backend_outs_wrapper };
  unsigned long state = __libc_backend_printf_lock();
  int ret = __v_printf(&_ap,format,ap);
  __libc_backend_printf_unlock(state);

  return ret;
}
