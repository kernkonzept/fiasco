IMPLEMENTATION [arm && pf_s32z]:

#include "infinite_loop.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  L4::infinite_loop();
}
