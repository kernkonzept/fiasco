IMPLEMENTATION [arm && pf_armadaa8k]:

#include "infinite_loop.h"
#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();

  L4::infinite_loop();
}
