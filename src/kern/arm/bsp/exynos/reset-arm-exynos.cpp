IMPLEMENTATION [arm && pf_exynos]:

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Io::write<Mword>(1, Kmem::mmio_remap(Mem_layout::Pmu_phys_base + 0x400));

  // we should reboot now
  while (1)
    ;
}
