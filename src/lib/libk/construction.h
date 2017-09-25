#ifndef INITFINI_H__
#define INITFINI_H__

#include <cdefs.h>

__BEGIN_DECLS

void static_construction(void);
void static_destruction(void);

typedef void (*ctor_function_t)(void);

void run_ctor_functions(ctor_function_t *start, ctor_function_t *end);

__END_DECLS



#endif // INITFINI_H__
