INTERFACE:

#include <cstddef>

IMPLEMENTATION:

#include <cstdio>
#include <cstdlib>
#include "panic.h"
#include "types.h"

char __dso_handle __attribute__((weak));

extern "C" void __cxa_pure_virtual() 
{
  panic("cxa pure virtual function called");
}

extern "C" void __pure_virtual()
{
  panic("pure virtual function called");
}

void operator delete(void *) throw()
{
  // This must not happen: We never delete an object of the abstract
  // class slab_cache_anon.  If the compiler was clever, it wouldn't
  // generate a call to this function (because all destructors of
  // abstract base classes have been marked abstract virtual), and
  // we wouldn't need to define this.
  panic("operator delete (aka __builtin_delete) called from " L4_PTR_FMT,
      L4_PTR_ARG(__builtin_return_address(0)));
}

extern "C" void __div0(void)
{
  panic("__div0");
}

