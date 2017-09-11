IMPLEMENTATION [arm && pf_imx_21]:

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum {
    WCR  = Kmem::Watchdog_phys_base + 0,
    WCR_SRS = 1 << 4, // Software Reset Signal

    PLL_PCCR1        = Kmem::Pll_phys_base + 0x24,
    PLL_PCCR1_WDT_EN = 1 << 24,
  };

  // WDT CLock Enable
  Io::set<Unsigned32>(PLL_PCCR1_WDT_EN, Kmem::mmio_remap(PLL_PCCR1));

  // Assert Software reset signal by making the bit zero
  Io::mask<Unsigned16>(~WCR_SRS, Kmem::mmio_remap(WCR));

  for (;;)
    ;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_28]:

#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Register_block<32> r(Kmem::mmio_remap(0x80056000));
  r[0x50] = 1; // Watchdog counter
  r[0x04] = 1 << 4; // Watchdog enable

  for (;;)
    ;
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
  Io::clear<Mword>(7 << 22, Kmem::mmio_remap(Mem_layout::Src_phys_base) + 0);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_7]:

void platform_imx_cpus_off()
{
  // switch off core1
  Io::clear<Mword>(1 << 1, Kmem::mmio_remap(Mem_layout::Src_phys_base) + 8);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_imx_35 || pf_imx_51 || pf_imx_53 || pf_imx_6
                        || pf_imx_6ul || pf_imx_7)]:

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum {
    WCR     = Mem_layout::Watchdog_phys_base + 0,
    WCR_WDE = 1 << 2,
  };

  platform_imx_cpus_off();

  // Enable watchdog with smallest timeout possible (0.5s)
  Io::modify<Unsigned16>(WCR_WDE, 0xff00, Kmem::mmio_remap(WCR));

  for (;;)
    ;
}
