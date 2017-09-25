#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

int vsprintf(char *dest,const char *format, va_list arg_ptr)
{
  return vsnprintf(dest,(size_t)-1,format,arg_ptr);
}
