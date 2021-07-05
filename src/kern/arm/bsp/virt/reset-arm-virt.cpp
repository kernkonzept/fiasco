IMPLEMENTATION [arm && pf_arm_virt]:

#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();

  for (;;)
    ;
}
