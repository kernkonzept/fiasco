IMPLEMENTATION [arm && pf_s3c2410]:

#include "infinite_loop.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block wdg(Kmem_mmio::map(Mem_layout::Watchdog_phys_base, 0x10));
  enum {
    WTCON = 0x0,
    WTDAT = 0x4,
    WTCNT = 0x8,

    WTCON_RST_EN    = 1 << 0,
    WTCON_EN        = 1 << 5,
    WTCON_PRESCALER = (0x10 << 8),
  };

  wdg.write<Unsigned32>(0, WTCON); // disable
  wdg.write<Unsigned32>(200, WTDAT); // set initial values
  wdg.write<Unsigned32>(200, WTCNT);

  wdg.write<Unsigned32>(WTCON_RST_EN | WTCON_EN | WTCON_PRESCALER, WTCON);

  // we should reboot now
  L4::infinite_loop();
}
