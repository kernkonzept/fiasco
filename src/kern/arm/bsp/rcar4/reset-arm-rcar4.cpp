IMPLEMENTATION [arm && pf_rcar4]:

#include "infinite_loop.h"
#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  L4::infinite_loop();
}
