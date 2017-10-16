IMPLEMENTATION [arm && pf_bcm283x]:

#include "io.h"
#include "kmem.h"

#include <cstdio>

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum { Rstc = 0x1c, Wdog = 0x24 };

  Address base = Kmem::mmio_remap(Mem_layout::Watchdog_phys_base);

  Mword pw = 0x5a << 24;
  Io::write<Unsigned32>(pw | 8, base + Wdog);
  Io::write<Unsigned32>((Io::read<Unsigned32>(base + Rstc) & ~0x30)
                        | pw | 0x20,
                        base + Rstc);

  for (;;)
    ;
}
