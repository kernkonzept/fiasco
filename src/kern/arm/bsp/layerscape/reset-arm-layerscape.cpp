IMPLEMENTATION [arm && pf_layerscape]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset(void)
{
  Mmio_register_block r(Kmem_mmio::map(0x02ad0000, sizeof(Unsigned16)));
  r.r<16>(0x0) = 1 << 2;

  L4::infinite_loop();
}
