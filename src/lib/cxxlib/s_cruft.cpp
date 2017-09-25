INTERFACE:

#include <cstddef>			// for size_t

IMPLEMENTATION:

#include "panic.h"			// for panic

// do not allow calls to these OSKit routines.  We have to provide
// these functions because libamm references them.
 
extern "C" __attribute__((noreturn)) void *smalloc(size_t)
{
  panic("smalloc called");
}
   
extern "C" __attribute__((noreturn)) void sfree(void *, size_t)
{
  panic("sfree called");
}
