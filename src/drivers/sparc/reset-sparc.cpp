IMPLEMENTATION [sparc && leon3]:

#include "io.h"

/**
 * Program General purpose timer as watchdog, thus causing a system reset
 */
void __attribute__ ((noreturn))
platform_reset(void)
{
  //in  case we return
  for(;;) ;
}

IMPLEMENTATION [sparc && !leon3]:

void __attribute__ ((noreturn))
platform_reset(void)
{
  for(;;);
}

