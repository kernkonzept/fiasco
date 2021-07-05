// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_ls1046 && arm_psci]:

#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  for (;;)
    ;
}
