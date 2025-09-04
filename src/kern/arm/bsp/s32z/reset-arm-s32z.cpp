IMPLEMENTATION [arm && pf_s32z]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset()
{
  L4::infinite_loop();
}
