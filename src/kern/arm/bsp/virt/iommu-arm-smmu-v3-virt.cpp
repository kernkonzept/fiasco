IMPLEMENTATION [arm && iommu && pf_arm_virt]:

#include "kmem_mmio.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Num_iommus == 1, "Unexpected number of IOMMUs.");
  Address base_addr = 0x09050000;

  void *v = Kmem_mmio::map(base_addr, 0x20000);
  iommus()[0].setup(v, 106, 109);

  return true;
}
