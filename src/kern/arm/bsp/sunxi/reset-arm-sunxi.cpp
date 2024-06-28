IMPLEMENTATION [arm && pf_sunxi]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  void *wdt = Kmem_mmio::map(0x01c20c90, 0x10);
  Io::write<Unsigned32>(3, offset_cast<void *>(wdt, 4));
  Io::write<Unsigned32>(1, wdt);

  L4::infinite_loop();
}
