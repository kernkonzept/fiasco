IMPLEMENTATION [ppc32 && mpc52xx]:

#include "io.h"
#include "boot_info.h"

enum Reg_offsets
{
  Gp0_mode  = 0x600,
  Gp0_count = 0x604
};

/**
 * Program General purpose timer as watchdog, thus causing a system reset
 */
void __attribute__ ((noreturn))
platform_reset(void)
{

  Address mbar = Boot_info::mbar();
  Io::write<Unsigned32>(0, mbar + Gp0_mode);
  Io::write<Unsigned32>(0xff, mbar + Gp0_count); //timeout

  //0x9004 (0x8000 enable watchdog | 0x1000 CE bit - start counter |
  //0x4 CE bit controls timer counter
  Io::write<Unsigned32>(0x9004, mbar + Gp0_mode);

  //in  case we return
  for(;;) ;
}

IMPLEMENTATION [ppc32 && !mpc52xx]:

void __attribute__ ((noreturn))
platform_reset(void)
{
  for(;;);
}

