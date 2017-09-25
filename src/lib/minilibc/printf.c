#include <stdarg.h>
#include <stdio.h>

int printf (const char *format, ...) {

  int n;
  va_list args;
  
  va_start    (args, format);
  n = vprintf (format, args);
  va_end      (args);
  
  return n;
}
