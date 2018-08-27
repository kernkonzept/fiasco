IMPLEMENTATION [arm && pf_armada37xx]:

#include "platform_control.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Platform_control::system_reset();

  for (;;)
    ;
}
