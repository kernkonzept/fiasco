IMPLEMENTATION [arm && iommu && pf_layerscape]:

#include "kmem.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Num_iommus == 4, "Unexpected number of IOMMUs.");
  /*
   * SMMU name                   | base addr  | ID for io
   * ----------------------------------------------------
   * SMMU1 (Interconnect fabric) | 0x01200000 | 0
   * SMMU2 (PCI express)         | 0x01280000 | 1
   * SMMU3 (eTSEC)               | 0x01300000 | 2
   * SMMU4 (qDMA)                | 0x01380000 | 3
   */
  Address base_addrs[] = { 0x01200000, 0x01280000, 0x01300000, 0x01380000 };
  unsigned nonsec_irqs[] = { 101, 103, 105, 206 };

  for (unsigned i = 0; i < Num_iommus; ++i)
    {
      Address v = Kmem::mmio_remap(base_addrs[i], 0x10000);
      iommus()[i].setup(Version::Smmu_v1, v);
      iommus()[i].setup_irqs(&nonsec_irqs[i], 1, 1);
    }

  return true;
}
