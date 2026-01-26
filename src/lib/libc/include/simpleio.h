#ifndef _SIMPLEIO_H
#define _SIMPLEIO_H

#include <cdefs.h>
#include <fiasco_defs.h>

#include "globalconfig.h"

__BEGIN_DECLS

#ifdef CONFIG_OUTPUT
int FIASCO_COLD putstr(const char *const s);
int FIASCO_COLD putnstr(const char *const c, int len);
#else
static inline int putstr(const char *const)
{ return 0; }
static inline int putnstr(const char *const, int)
{ return 0; }
#endif

__END_DECLS

#endif
