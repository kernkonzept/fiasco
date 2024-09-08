#include <ctype.h>
#undef isprint

int isprint(int c)
{
	return (unsigned)c-0x20 < 0x5f;
}

#ifndef LIBCL4
int __isprint_l(int c, locale_t l)
{
	return isprint(c);
}

weak_alias(__isprint_l, isprint_l);
#endif /* LIBCL4 */
