#include "stdio_impl.h"
#include <string.h>

size_t __fwritex(const char *restrict s, size_t l, FILE *restrict f)
{
	size_t i=0;

	if (!f->wend)
		__towrite(f);

	if (l > (size_t)(f->wend - f->wpos))
		return f->write(f, s, l);

	memcpy(f->wpos, s, l);
	f->wpos += l;
	return l+i;
}
