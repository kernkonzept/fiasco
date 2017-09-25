INTERFACE [arm]:

#include "types.h"

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstdlib>
#include <cstdio>
#include <construction.h>
#include "terminate.h"

void kernel_main(void);

extern "C"
void __main()
{
  atexit(&static_destruction);
  static_construction();
  kernel_main();
  terminate(0);
}
