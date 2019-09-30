IMPLEMENTATION [arm && arm_psci]:

#include "psci.h"

PUBLIC static
int
Platform_control::cpu_on(unsigned long target, Address phys_tramp_mp_addr)
{
  return Psci::cpu_on(target, phys_tramp_mp_addr);
}

PUBLIC static
void
Platform_control::system_reset()
{
  Psci::system_reset();
}

IMPLEMENT_OVERRIDE
void
Platform_control::system_off()
{
  Psci::system_off();
}
