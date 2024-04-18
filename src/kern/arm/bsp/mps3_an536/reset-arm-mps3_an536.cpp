IMPLEMENTATION [arm && pf_mps3_an536]:

#include "infinite_loop.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  L4::infinite_loop();
}
