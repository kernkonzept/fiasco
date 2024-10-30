IMPLEMENTATION [arm && pf_zynq]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset(void)
{
  Mmio_register_block slcr(Kmem_mmio::map(0xf8000000, 0x1000));
  slcr.write<Unsigned32>(0xdf0d, 0x8);
  slcr.write<Unsigned32>(1, 0x200);

  L4::infinite_loop();
}
