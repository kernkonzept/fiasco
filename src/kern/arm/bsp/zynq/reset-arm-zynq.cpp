IMPLEMENTATION [arm && zynq]:

#include "io.h"
#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block slcr(Kmem::mmio_remap(0xf8000000));
  slcr.write<Unsigned32>(0xdf0d, 0x8);
  slcr.write<Unsigned32>(1, 0x200);

  for (;;)
    ;
}
