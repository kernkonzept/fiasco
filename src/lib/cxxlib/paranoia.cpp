INTERFACE:

#include <cstddef>

// Required for Clang which lacks a builtin definition of std::align_val_t.
namespace std { enum class align_val_t : size_t {}; }

IMPLEMENTATION:

#include <cstdio>
#include <cstdlib>
#include "panic.h"
#include "types.h"

char __dso_handle __attribute__((weak));

extern "C" void __cxa_pure_virtual() 
{
  panic("cxa pure virtual function called from " L4_PTR_FMT,
        L4_PTR_ARG(__builtin_return_address(0)));
}

extern "C" void __pure_virtual()
{
  panic("pure virtual function called from " L4_PTR_FMT,
        L4_PTR_ARG(__builtin_return_address(0)));
}

void operator delete(void *) throw()
{
  // This must not happen: We never delete an object of the abstract class
  // slab_cache_anon.  If the compiler was clever, it wouldn't generate a call
  // to this function (because all destructors of abstract base classes have
  // been marked abstract virtual), and we wouldn't need to define this.
  panic("operator delete (aka __builtin_delete) called from " L4_PTR_FMT,
      L4_PTR_ARG(__builtin_return_address(0)));
}

void operator delete(void *, std::align_val_t)
{
  // This must not happen -- same as above.
  //
  // According to C++17, the compiler will call this function instead of the
  // function above if the alignment of the object to be deleted exceeds the
  // value of __STDCPP_DEFAULT_NEW_ALIGNMENT__.
  //
  // So far this was only seen on SPARC with
  //  * __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 8 and
  //  * alignof(Context) == 16.
  //
  // Clang doesn't provide std::align_val_t.
  panic("operator delete (aka __builtin_delete) called from " L4_PTR_FMT,
      L4_PTR_ARG(__builtin_return_address(0)));
}

extern "C" void __div0(void)
{
  panic("__div0");
}

