IMPLEMENTATION [arm && pf_arm_gen]:

#include "infinite_loop.h"
#include "psci.h"
#include <cstdio>

void __attribute__ ((noreturn))
platform_reset(void)
{
  if (Psci::is_detected())
    Psci::system_reset();
  else
    printf("No way to reset the system.\n");

  L4::infinite_loop();
}
