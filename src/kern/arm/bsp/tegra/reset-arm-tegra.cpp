IMPLEMENTATION [arm && pf_tegra]:

#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block b(Kmem::mmio_remap(Mem_layout::Pmc_phys_base));
  b.modify<Mword>(0x10, 0, 0);
  for (;;)
    ;
}
