#include "stdio_impl.h"
#include <string.h>

size_t __fwritex(const unsigned char *restrict s, size_t l, FILE *restrict f)
{
	size_t i=0;

	if (!f->wend && __towrite(f)) return 0;

	if (l > (size_t)(f->wend - f->wpos)) return f->write(f, s, l);

	memcpy(f->wpos, s, l);
	f->wpos += l;
	return l+i;
}

size_t fwrite(const void *restrict src, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t k, l = size*nmemb;
	if (!size) nmemb = 0;
	FLOCK(f);
	k = __fwritex(src, l, f);
	FUNLOCK(f);
	return k==l ? nmemb : k/size;
}

#ifndef LIBCL4
weak_alias(fwrite, fwrite_unlocked);
#endif /* LIBCL4 */
