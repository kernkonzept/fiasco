IMPLEMENTATION [arm && pf_armadaa8k]:

#include "platform_control.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Platform_control::system_reset();

  L4::infinite_loop();
}
