IMPLEMENTATION [arm && pf_rpi && !arm_psci]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

#include <cstdio>

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum { Rstc = 0x1c, Wdog = 0x24 };

  Address base = Kmem_mmio::remap(Mem_layout::Watchdog_phys_base, 0x100);

  Mword pw = 0x5a << 24;
  Io::write<Unsigned32>(pw | 8, base + Wdog);
  Io::write<Unsigned32>((Io::read<Unsigned32>(base + Rstc) & ~0x30)
                        | pw | 0x20,
                        base + Rstc);

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && arm_psci]:

#include "infinite_loop.h"
#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  L4::infinite_loop();
}
