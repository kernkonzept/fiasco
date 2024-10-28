IMPLEMENTATION [arm && iommu && pf_imx_95]:

#include "kmem_mmio.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Num_iommus == 1, "Unexpected number of IOMMUs.");

  void *v = Kmem_mmio::map(0x490d0000, 0x100000);
  iommus()[0].setup(v, 0x165, 0x168);

  return true;
}
