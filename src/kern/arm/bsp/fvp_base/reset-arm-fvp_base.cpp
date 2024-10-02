IMPLEMENTATION [arm && pf_fvp_base]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset(void)
{
  L4::infinite_loop();
}
