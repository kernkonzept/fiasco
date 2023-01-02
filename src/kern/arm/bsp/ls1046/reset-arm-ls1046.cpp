// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_ls1046 && arm_psci]:

#include "infinite_loop.h"
#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  L4::infinite_loop();
}
