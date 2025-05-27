#include "stdio_impl.h"

int __towrite(FILE *f)
{
	/* Clear read buffer (easier than summoning nasal demons) */
	f->rpos = 0;

	/* Activate write through the buffer. */
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;

	return 0;
}
