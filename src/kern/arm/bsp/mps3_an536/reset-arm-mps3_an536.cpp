IMPLEMENTATION [arm && pf_mps3_an536]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset(void)
{
  L4::infinite_loop();
}
