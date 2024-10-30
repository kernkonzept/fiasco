IMPLEMENTATION [arm && pf_exynos]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset(void)
{
  Io::write<Mword>(1, Kmem_mmio::map(Mem_layout::Pmu_phys_base + 0x400,
                                     sizeof(Mword)));

  // we should reboot now
  L4::infinite_loop();
}
