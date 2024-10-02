IMPLEMENTATION [mips && baikal_t]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset(void)
{
  Register_block<32> r(Kmem_mmio::map(0x1f04c000, 0x10));
  r[0x0] = r[0] & ~3;
  r[0x4] = 0;
  r[0x0] = r[0] | 1;
  r[0xc] = 0x76;

  L4::infinite_loop();
}
