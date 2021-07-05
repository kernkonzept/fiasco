IMPLEMENTATION [arm && pf_armada37xx]:

#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();

  for (;;)
    ;
}
