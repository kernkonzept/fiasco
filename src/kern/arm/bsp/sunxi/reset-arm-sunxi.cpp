IMPLEMENTATION [arm && pf_sunxi]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Address wdt = Kmem_mmio::remap(0x01c20c90, 0x10);
  Io::write<Unsigned32>(3, wdt + 4);
  Io::write<Unsigned32>(1, wdt + 0);

  L4::infinite_loop();
}
