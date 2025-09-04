IMPLEMENTATION [arm && pf_rpi && !arm_psci]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

#include <cstdio>

[[noreturn]] void
platform_reset()
{
  enum { Rstc = 0x1c, Wdog = 0x24 };

  void *base = Kmem_mmio::map(Mem_layout::Watchdog_phys_base, 0x100);

  void *wdog_ptr = offset_cast<void *>(base, Wdog);
  Mword pw = 0x5a << 24;
  Io::write<Unsigned32>(pw | 8, wdog_ptr);

  void *rstc_ptr = offset_cast<void *>(base, Rstc);
  Unsigned32 rstc = Io::read<Unsigned32>(rstc_ptr);
  Io::write<Unsigned32>((rstc & ~0x30) | pw | 0x20, rstc_ptr);

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && arm_psci]:

#include "infinite_loop.h"
#include "psci.h"

[[noreturn]] void
platform_reset()
{
  Psci::system_reset();
  L4::infinite_loop();
}
