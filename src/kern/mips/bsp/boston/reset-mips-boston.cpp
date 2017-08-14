IMPLEMENTATION [mips && boston]:

#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Register_block<32> syscon(Kmem::mmio_remap(0x17ffd010));
  syscon[0x10] = 0x10;

  for (;;)
    ;
}
