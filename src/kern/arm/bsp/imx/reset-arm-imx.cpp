IMPLEMENTATION [arm && pf_imx_21]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset(void)
{
  enum {
    WCR  = Kmem::Watchdog_phys_base + 0,
    WCR_SRS = 1 << 4, // Software Reset Signal

    PLL_PCCR1        = Kmem::Pll_phys_base + 0x24,
    PLL_PCCR1_WDT_EN = 1 << 24,
  };

  // WDT CLock Enable
  Io::set<Unsigned32>(PLL_PCCR1_WDT_EN, Kmem_mmio::map(PLL_PCCR1,
                                                       sizeof(Unsigned32)));

  // Assert Software reset signal by making the bit zero
  Io::mask<Unsigned16>(~WCR_SRS, Kmem_mmio::map(WCR, sizeof(Unsigned16)));

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_28]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset(void)
{
  Register_block<32> r(Kmem_mmio::map(0x80056000, 0x100));
  r[0x50] = 1; // Watchdog counter
  r[0x04] = 1 << 4; // Watchdog enable

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_imx_35 || pf_imx_51 || pf_imx_53 || pf_imx_6ul)]:

void platform_imx_cpus_off()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_6]:

void platform_imx_cpus_off()
{
  // switch off core1-3
  void *ptr = Kmem_mmio::map(Mem_layout::Src_phys_base, sizeof(Mword));
  Io::clear<Mword>(7 << 22, ptr);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_7]:

void platform_imx_cpus_off()
{
  // switch off core1
  void *ptr = Kmem_mmio::map(Mem_layout::Src_phys_base, sizeof(Mword));
  Io::clear<Mword>(1 << 1, offset_cast<void *>(ptr, 8));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_imx_35 || pf_imx_51 || pf_imx_53 || pf_imx_6
                        || pf_imx_6ul || pf_imx_7)]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset(void)
{
  enum {
    WCR     = Mem_layout::Watchdog_phys_base + 0,
    WCR_WDE = 1 << 2,
  };

  platform_imx_cpus_off();

  // Enable watchdog with smallest timeout possible (0.5s)
  Io::modify<Unsigned16>(WCR_WDE, 0xff10, Kmem_mmio::map(WCR,
                                                         sizeof(Unsigned16)));

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

#include "infinite_loop.h"
#include "psci.h"

[[noreturn]] void
platform_reset(void)
{
  Psci::system_reset();
  L4::infinite_loop();
}
