INTERFACE [arm]:

#include "types.h"

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstdlib>
#include <cstdio>
#include <construction.h>

[[noreturn]] void kernel_main(void);

extern "C"
[[noreturn]] void __main()
{
  static_construction();
  kernel_main();
}
