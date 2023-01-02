IMPLEMENTATION [sparc && leon3]:

#include "infinite_loop.h"
#include "io.h"

/**
 * Program General purpose timer as watchdog, thus causing a system reset
 */
void __attribute__ ((noreturn))
platform_reset(void)
{
  //in  case we return
  L4::infinite_loop();
}

IMPLEMENTATION [sparc && !leon3]:

void __attribute__ ((noreturn))
platform_reset(void)
{
  L4::infinite_loop();
}

