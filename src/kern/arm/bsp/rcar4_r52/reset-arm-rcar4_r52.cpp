IMPLEMENTATION [arm && pf_rcar4_r52]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset()
{
  L4::infinite_loop();
}
