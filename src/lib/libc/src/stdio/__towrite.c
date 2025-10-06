#include "stdio_impl.h"

void __towrite(FILE *f)
{
	/* Clear read buffer (easier than summoning nasal demons) */
	f->rpos = 0;

	/* Activate write through the buffer. */
	f->wpos = f->buf;
	f->wend = f->buf + f->buf_size;
}
