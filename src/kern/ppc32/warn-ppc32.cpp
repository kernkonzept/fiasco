INTERFACE [ppc32]:
#include "panic.h"

#define NOT_IMPL WARN "%s not implemented", __PRETTY_FUNCTION__
#define NOT_IMPL_PANIC panic("%s not implemented\n", __PRETTY_FUNCTION__);
