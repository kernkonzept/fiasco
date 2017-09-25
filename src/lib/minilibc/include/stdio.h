#ifndef _STDIO_H
#define _STDIO_H

#include <cdefs.h>
#include <stddef.h>
#include <mini_defs.h>
#include <fiasco_defs.h>

__BEGIN_DECLS

int FIASCO_COLD putchar(int c);
int FIASCO_COLD puts(const char *s);
int FIASCO_COLD printf(const char *format, ...)
  __attribute__((format(printf,1,2)));
int FIASCO_COLD sprintf(char *str, const char *format, ...)
  __attribute__((format(printf,2,3)));
int FIASCO_COLD snprintf(char *str, size_t size, const char *format, ...)
  __attribute__((format(printf,3,4)));

#include <stdarg.h>

int FIASCO_COLD vprintf(const char *format, va_list ap)
  __attribute__((format(printf,1,0)));
int FIASCO_COLD vsprintf(char *str, const char *format, va_list ap)
  __attribute__((format(printf,2,0)));
int FIASCO_COLD vsnprintf(char *str, size_t size, const char *format, va_list ap)
  __attribute__((format(printf,3,0)));

typedef int *FILE;

__END_DECLS

#endif
