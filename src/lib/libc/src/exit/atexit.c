#include <stdlib.h>
#include <stdint.h>

int __cxa_atexit(void (*func)(void *), void *arg, void *dso);

int __cxa_atexit(void (*func)(void *), void *arg, void *dso)
{
	(void)func;
	(void)arg;
	(void)dso;

	return 0;
}

