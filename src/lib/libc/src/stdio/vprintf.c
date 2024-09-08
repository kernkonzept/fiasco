#include <stdio.h>

int vprintf(const char *restrict fmt, va_list ap)
{
	return vfprintf(libc_stdout, fmt, ap);
}
