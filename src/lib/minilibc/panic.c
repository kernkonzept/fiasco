#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <panic.h>
#include "types.h"

void __attribute__((weak))
panic (const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  exit (EXIT_FAILURE);
}
