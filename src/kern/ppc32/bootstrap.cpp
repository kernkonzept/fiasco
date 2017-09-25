INTERFACE [ppc32]:
#include "types.h"
#include "initcalls.h"

IMPLEMENTATION [ppc32]:
#include "uart.h"
#include "boot_info.h"
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
  Boot_info::set_prom(prom_ptr);
  atexit(&static_destruction);
  static_construction();
  printf("Bootstrapped\n");
  terminate(main());
  return 0;
}

