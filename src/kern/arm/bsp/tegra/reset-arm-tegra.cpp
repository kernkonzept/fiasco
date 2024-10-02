IMPLEMENTATION [arm && pf_tegra]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset(void)
{
  Mmio_register_block b(Kmem_mmio::map(Mem_layout::Pmc_phys_base,
                                       sizeof(Mword)));
  b.modify<Mword>(0x10, 0, 0);
  L4::infinite_loop();
}
