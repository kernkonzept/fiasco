IMPLEMENTATION [mips && sead3]:

#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Register_block<32> r(Kmem::mmio_remap(0x1f000050));
  r[0] = 0x4d;

  for (;;)
    ;
}
