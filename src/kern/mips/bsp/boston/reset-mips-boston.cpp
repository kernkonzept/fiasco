IMPLEMENTATION [mips && boston]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Register_block<32> syscon(Kmem_mmio::remap(0x17ffd010, 0x20));
  syscon[0x10] = 0x10;

  L4::infinite_loop();
}
