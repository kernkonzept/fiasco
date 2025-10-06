#include "stdio_impl.h"
#include <limits.h>
#include <string.h>
#include <stdint.h>

struct cookie {
	char *s;
	size_t n;
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void sn_write(FILE *f, const char *s, size_t l)
{
	struct cookie *c = f->cookie;
	size_t k = MIN(c->n, (size_t)(f->wpos - f->buf));
	if (k) {
		memcpy(c->s, f->buf, k);
		c->s += k;
		c->n -= k;
	}
	k = MIN(c->n, l);
	if (k) {
		memcpy(c->s, s, k);
		c->s += k;
		c->n -= k;
	}
	*c->s = 0;
	f->wpos = f->buf;
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
	char buf[1];
	char dummy[1];
	struct cookie c = { .s = n ? s : dummy, .n = n ? n-1 : 0 };
	FILE f = {
		.write = sn_write,
		.buf = buf,
		.cookie = &c,
	};

	*c.s = 0;
	return vfprintf(&f, fmt, ap);
}
