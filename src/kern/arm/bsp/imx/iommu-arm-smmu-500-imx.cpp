IMPLEMENTATION [arm && iommu && pf_imx_8xq]:

#include "kmem.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Num_iommus == 1, "Unexpected number of IOMMUs.");
  unsigned nonsec_irqs[] =
  {
    // Global/Context Irq
    64,
  };

  Address v = Kmem::mmio_remap(0x51400000, 0x40000);
  iommus()[0].setup(Version::Smmu_v2, v, 0x7f80);
  iommus()[0].setup_irqs(nonsec_irqs, 1, 1);

  return true;
}
