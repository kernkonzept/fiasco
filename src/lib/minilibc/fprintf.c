#include <stdarg.h>
#include <stdio.h>

int fprintf(FILE *stream, const char *format, ...)
{
  int n;
  va_list args;
  (void)stream;
  
  va_start    (args, format);
  n = vprintf (format, args);
  va_end      (args);
  
  return n;
}

