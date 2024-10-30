IMPLEMENTATION [arm && pf_armada38x]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset(void)
{
  // Configuration and Control
  Mmio_register_block r(Kmem_mmio::map(0xf1018200, 0x100));
  r.r<32>(0x60) = 1;
  r.r<32>(0x64) = 1;

  L4::infinite_loop();
}
