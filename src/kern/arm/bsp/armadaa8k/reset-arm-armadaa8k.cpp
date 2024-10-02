IMPLEMENTATION [arm && pf_armadaa8k]:

#include "infinite_loop.h"
#include "psci.h"

[[noreturn]] void
platform_reset(void)
{
  Psci::system_reset();

  L4::infinite_loop();
}
