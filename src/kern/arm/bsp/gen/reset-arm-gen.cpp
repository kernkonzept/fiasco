IMPLEMENTATION [arm && pf_arm_gen]:

#include "infinite_loop.h"
#include "psci.h"
#include <cstdio>

void __attribute__ ((noreturn))
platform_reset(void)
{
  if (Psci::is_detected())
    Psci::system_reset();
  else if (!platform_reset_dt())
    printf("No way to reset the system.\n");

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && dt]:

#include "dt.h"
#include "io.h"
#include "kmem_mmio.h"

bool platform_reset_dt()
{
  if (!Dt::have_fdt())
    return false;

  Unsigned64 addr;
  Dt::Node bcm2711_pm = Dt::node_by_compatible("brcm,bcm2711-pm");
  if (bcm2711_pm.is_valid() && bcm2711_pm.get_reg(0, &addr))
    {
      enum
      {
        Rstc = 0x1c,
        Wdog = 0x24
      };

      void *base = Kmem_mmio::map(addr, 0x100);

      void *wdog_ptr = offset_cast<void *>(base, Wdog);
      Mword pw = 0x5a << 24;
      Io::write<Unsigned32>(pw | 8, wdog_ptr);

      void *rstc_ptr = offset_cast<void *>(base, Rstc);
      Unsigned32 rstc = Io::read<Unsigned32>(rstc_ptr);
      Io::write<Unsigned32>((rstc & ~0x30) | pw | 0x20, rstc_ptr);
      return true;
    }

  return false;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !dt]:

bool platform_reset_dt()
{ return false; }
