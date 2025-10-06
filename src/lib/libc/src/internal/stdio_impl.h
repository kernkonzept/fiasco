#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>
#include "libc_backend.h"

#define FLOCK(f) unsigned long lock_state = __libc_backend_printf_lock();
#define FUNLOCK(f) __libc_backend_printf_unlock(lock_state)

struct _IO_FILE {
	char *rpos;
	char *wend, *wpos;
	void (*write)(FILE *, const char *, size_t);
	char *buf;
	size_t buf_size;
	void *cookie;
};

hidden void __towrite(FILE *);
hidden int vfprintf(FILE *__restrict, const char *__restrict, __isoc_va_list);

#endif
