IMPLEMENTATION [arm && pf_armada38x]:

#include "io.h"
#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block r(Kmem::mmio_remap(0xf1018200));
  r.r<32>(0x60) = 1;
  r.r<32>(0x64) = 1;

  for (;;)
    ;
}
