#include "stdio_impl.h"

int __towrite(FILE *f)
{
#ifndef LIBCL4
	f->mode |= f->mode-1;
#endif
	if (f->flags & F_NOWR) {
		f->flags |= F_ERR;
		return EOF;
	}
	/* Clear read buffer (easier than summoning nasal demons) */
	f->rpos = 0;

	/* Activate write through the buffer. */
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;

	return 0;
}

#ifndef LIBCL4
hidden void __towrite_needs_stdio_exit()
{
	__stdio_exit_needed();
}
#endif /* LIBCL4 */
