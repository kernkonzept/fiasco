#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>
#include "libc_backend.h"

struct _IO_FILE {
	char *rpos;
	char *wpos;
	void (*write)(FILE *, const char *, size_t);
	char *buf;
	size_t buf_size;
	void *cookie;
};

hidden int vfprintf(FILE *__restrict, const char *__restrict, __isoc_va_list);

#endif
