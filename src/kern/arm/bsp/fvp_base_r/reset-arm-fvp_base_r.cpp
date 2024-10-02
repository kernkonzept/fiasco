IMPLEMENTATION [arm && pf_fvp_base_r]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset(void)
{
  L4::infinite_loop();
}
