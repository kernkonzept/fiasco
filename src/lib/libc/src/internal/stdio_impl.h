#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>
#include "libc_backend.h"

#define FLOCK(f) unsigned long lock_state = __libc_backend_printf_lock();
#define FUNLOCK(f) __libc_backend_printf_unlock(lock_state)

struct _IO_FILE {
	unsigned char *rpos;
	unsigned char *wend, *wpos;
	unsigned char *wbase;
	size_t (*write)(FILE *, const unsigned char *, size_t);
	unsigned char *buf;
	size_t buf_size;
	void *cookie;
};

hidden int __towrite(FILE *);

hidden size_t __fwritex(const unsigned char *, size_t, FILE *);

#endif
