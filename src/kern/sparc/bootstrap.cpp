INTERFACE [sparc]:
#include "types.h"
#include "initcalls.h"

IMPLEMENTATION [sparc]:
#include "uart.h"
#include "terminate.h"

#include <construction.h>
#include <cstdlib>
#include <cstdio>

extern "C" int main(void);

extern "C" FIASCO_INIT
int bootstrap_main(void *, Address)
{
  atexit(&static_destruction);
  static_construction();
  puts("Bootstrapped");
  terminate(main());
  return 0;
}
