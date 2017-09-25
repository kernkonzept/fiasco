INTERFACE [watchdog]:

#include "initcalls.h"
#include "mmio_register_block.h"
#include "kmem.h"
#include "l4_types.h"

class Watchdog : private Mmio_register_block
{
private:
  enum {
    WTCON = 0x0,
    WTDAT = 0x4,
    WTCNT = 0x8,

    WTCON_RST_EN    = 1 << 0,
    WTCON_EN        = 1 << 5,
    WTCON_PRESCALER = (0x10 << 8),

    Reset_timeout_val = 299999 * 1000,
  };

  static Static_object<Watchdog> wdog;

public:
  Watchdog(Address virt) : Mmio_register_block(virt) {}
};

IMPLEMENTATION [watchdog]:

#include "io.h"
#include "static_init.h"

#include <cstdio>

Static_object<Watchdog> Watchdog::wdog;

PUBLIC static
void
Watchdog::enable()
{
  wdog->write(wdog->read<Mword>(WTCON) | WTCON_EN, WTCON);
}

PUBLIC static
void
Watchdog::disable()
{
  wdog->write(wdog->read<Mword>(WTCON) & ~WTCON_EN, WTCON);
}

PUBLIC static
void
Watchdog::touch()
{
  wdog->write<Mword>(Reset_timeout_val, WTCNT);
}

PUBLIC static
void
Watchdog::setup(Mword val)
{
  wdog->write<Mword>(0, WTCON); // disable
  wdog->write<Mword>(val, WTDAT); // set initial values
  wdog->write<Mword>(val, WTCNT);

  wdog->write<Mword>(WTCON_RST_EN | WTCON_PRESCALER, WTCON);
}

PUBLIC static FIASCO_INIT
void
Watchdog::init()
{
  wdog.construct(Kmem::mmio_remap(Mem_layout::Watchdog_phys_base));
  if (1)
    {
      wdog->setup(Reset_timeout_val);
      printf("Watchdog initialized\n");
    }
  else
    printf("Watchdog NOT running\n");
}

STATIC_INITIALIZE_P(Watchdog, WATCHDOG_INIT);
