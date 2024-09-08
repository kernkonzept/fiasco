#include "stdio_impl.h"

int puts(const char *s)
{
	int r;
	FLOCK(stdout);
	r = -(fputs(s, libc_stdout) < 0 || putc_unlocked('\n', libc_stdout) < 0);
	FUNLOCK(stdout);
	return r;
}
