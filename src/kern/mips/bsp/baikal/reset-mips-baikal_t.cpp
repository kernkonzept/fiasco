IMPLEMENTATION [mips && baikal_t]:

#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Register_block<32> r(Kmem::mmio_remap(0x1f04c000));
  r[0x0] = r[0] & ~3;
  r[0x4] = 0;
  r[0x0] = r[0] | 1;
  r[0xc] = 0x76;

  for (;;)
    ;
}
