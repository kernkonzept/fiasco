IMPLEMENTATION [arm && pf_arm_virt]:

#include "infinite_loop.h"
#include "psci.h"

[[noreturn]] void
platform_reset(void)
{
  Psci::system_reset();

  L4::infinite_loop();
}
