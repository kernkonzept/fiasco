#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <strutil.h>

char *__stpcpy(char *restrict d, const char *restrict s)
{
#ifdef __GNUC__
	typedef size_t __attribute__((__may_alias__)) word;
	word *wd;
	const word *ws;
	if ((uintptr_t)s % SS == (uintptr_t)d % SS) {
		for (; (uintptr_t)s % SS; s++, d++)
			if (!(*d=*s))
				return d;
		wd=(void *)d; ws=(const void *)s;
		for (; !HASZERO(*ws); *wd++ = *ws++);
		d=(void *)wd; s=(const void *)ws;
	}
#endif
	for (; (*d=*s); s++, d++);

	return d;
}

weak_alias(__stpcpy, stpcpy);
