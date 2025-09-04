IMPLEMENTATION [arm && pf_sr6p7g7]:

#include "infinite_loop.h"

[[noreturn]] void
platform_reset()
{
  L4::infinite_loop();
}
