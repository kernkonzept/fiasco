#ifndef __MC_ALLOCA__
#define __MC_ALLOCA__
#include <stddef.h>
#include <cdefs.h>

__BEGIN_DECLS

static inline void *alloca(size_t sz) { return __builtin_alloca(sz); }

__END_DECLS


#endif //__MC_ALLOCA__
