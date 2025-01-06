IMPLEMENTATION [arm && iommu && pf_imx_8xq]:

#include "kmem_mmio.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Num_iommus >= 1, "Unexpected number of IOMMUs.");
  unsigned const nonsec_irqs[] =
  {
    // Global/Context Irq
    64,
  };

  _iommus = Iommu_array(new Boot_object<Iommu>[1], 1);

  void *v = Kmem_mmio::map(0x51400000, 0x40000);
  _iommus[0].setup(Version::Smmu_v2, v, 0x7f80);
  _iommus[0].setup_irqs(nonsec_irqs, 1, 1);

  return true;
}
