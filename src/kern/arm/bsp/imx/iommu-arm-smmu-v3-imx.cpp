IMPLEMENTATION [arm && iommu && pf_imx_95]:

#include "kmem_mmio.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Max_iommus >= 1, "Unexpected number of IOMMUs.");

  _iommus = Iommu_array(new Boot_object<Iommu>[1], 1);

  void *v = Kmem_mmio::map(0x490d0000, 0x100000);
  _iommus[0].setup(v, 0x165, 0x168);

  return true;
}
