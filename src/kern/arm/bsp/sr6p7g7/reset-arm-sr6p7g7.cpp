IMPLEMENTATION [arm && pf_sr6p7g7]:

#include "infinite_loop.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  L4::infinite_loop();
}
