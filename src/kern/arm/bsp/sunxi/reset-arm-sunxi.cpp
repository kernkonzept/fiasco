IMPLEMENTATION [arm && sunxi]:

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Address wdt = Kmem::mmio_remap(0x01c20c90);
  Io::write<Unsigned32>(3, wdt + 4);
  Io::write<Unsigned32>(1, wdt + 0);

  for (;;)
    ;
}
