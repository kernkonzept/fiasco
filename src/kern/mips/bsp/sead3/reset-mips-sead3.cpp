IMPLEMENTATION [mips && sead3]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Register_block<32> r(Kmem_mmio::remap(0x1f000050, sizeof(Unsigned32)));
  r[0] = 0x4d;

  L4::infinite_loop();
}
