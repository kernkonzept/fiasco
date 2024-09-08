#include "stdio_impl.h"
#include "fiasco_stdio.h"

#undef stdout

static unsigned char buf[BUFSIZ+UNGET];
hidden FILE __stdout_FILE __attribute__((section(".data.no_rw_checksum"))) = {
	.buf = buf+UNGET,
	.buf_size = sizeof buf-UNGET,
	.fd = 1,
	.flags = F_PERM | F_NORD,
	.lbf = '\n',
#ifndef LIBCL4
	.write = __stdout_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
#else /* LIBCL4 */
	.write = __fiasco_stdout_write,
#endif /* LIBCL4 */
	.lock = -1,
};
#ifndef LIBCL4
FILE *const stdout = &__stdout_FILE;
#endif /* LIBCL4 */
FILE *volatile __stdout_used = &__stdout_FILE;
