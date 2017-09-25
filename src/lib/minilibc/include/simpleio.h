#ifndef _SIMPLEIO_H
#define _SIMPLEIO_H

#include <cdefs.h>
#include <fiasco_defs.h>

__BEGIN_DECLS

int FIASCO_COLD putstr(const char *const s);
int FIASCO_COLD putnstr(const char *const c, int len);

__END_DECLS

#endif
