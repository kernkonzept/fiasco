#include "stdio_impl.h"
#include <string.h>

size_t __fwritex(const unsigned char *restrict s, size_t l, FILE *restrict f)
{
	size_t i=0;

	if (!f->wend && __towrite(f))
		return 0;

	if (l > (size_t)(f->wend - f->wpos))
		return f->write(f, s, l);

	memcpy(f->wpos, s, l);
	f->wpos += l;
	return l+i;
}
