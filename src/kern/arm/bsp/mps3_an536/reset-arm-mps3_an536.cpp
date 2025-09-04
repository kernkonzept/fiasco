IMPLEMENTATION [arm && pf_mps3_an536]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset()
{
  L4::infinite_loop();
}
