#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <panic.h>
#include "types.h"

void __attribute__((weak))
__assert_fail(const char *__assertion, const char *__file,
              unsigned int __line, void *ret)
{
  printf("\nAssertion failed: '%s' [ret=%p]\n"
         "  %s:%u at " L4_PTR_FMT "\n",
         __assertion, ret, __file, __line,
          (Address)__builtin_return_address(0));

  exit(1);
}

void __attribute__((weak))
panic (const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  exit (EXIT_FAILURE);
}
