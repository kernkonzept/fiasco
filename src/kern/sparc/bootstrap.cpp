INTERFACE [sparc]:
#include "types.h"
#include "initcalls.h"

IMPLEMENTATION [sparc]:
#include "uart.h"
#include "terminate.h"

#include <construction.h>
#include <cstdlib>
#include <cstdio>

//XXX cbass: implement me
extern "C" void __attribute__ ((noreturn))
_exit(int)
{
  printf("Exiting\n");
  while(1){};
}

extern "C" int main(void);

extern "C" FIASCO_INIT
int bootstrap_main(void * /* r3 */, Address prom_ptr /* r4 */)
{
  atexit(&static_destruction);
  static_construction();
  puts("Bootstrapped");
  terminate(main());
  return 0;
}

