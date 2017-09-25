IMPLEMENTATION [arm && s3c2410]:

#include "mmio_register_block.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block wdg(Kmem::mmio_remap(Mem_layout::Watchdog_phys_base));
  enum {
    WTCON = 0x0,
    WTDAT = 0x4,
    WTCNT = 0x8,

    WTCON_RST_EN    = 1 << 0,
    WTCON_EN        = 1 << 5,
    WTCON_PRESCALER = (0x10 << 8),
  };

  wdg.write(0, WTCON); // disable
  wdg.write(200, WTDAT); // set initial values
  wdg.write(200, WTCNT);

  wdg.write(WTCON_RST_EN | WTCON_EN | WTCON_PRESCALER, WTCON);

  // we should reboot now
  while (1)
    ;
}
