IMPLEMENTATION [arm && pf_rcar3]:

#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  for (;;)
    ;
}
