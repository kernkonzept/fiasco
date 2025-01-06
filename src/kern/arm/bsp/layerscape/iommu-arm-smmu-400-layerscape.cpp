IMPLEMENTATION [arm && iommu && pf_layerscape]:

#include "kmem_mmio.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  static_assert(Max_iommus >= 4, "Unexpected number of IOMMUs.");
  /*
   * SMMU name                   | base addr  | ID for io
   * ----------------------------------------------------
   * SMMU1 (Interconnect fabric) | 0x01200000 | 0
   * SMMU2 (PCI express)         | 0x01280000 | 1
   * SMMU3 (eTSEC)               | 0x01300000 | 2
   * SMMU4 (qDMA)                | 0x01380000 | 3
   */
  Address const base_addrs[] = { 0x01200000, 0x01280000, 0x01300000, 0x01380000 };
  unsigned const nonsec_irqs[] = { 101, 103, 105, 206 };

  _iommus = Iommu_array(new Boot_object<Iommu>[4], 4);

  for (unsigned i = 0; i < 4; ++i)
    {
      void *v = Kmem_mmio::map(base_addrs[i], 0x10000);
      _iommus[i].setup(Version::Smmu_v1, v);
      _iommus[i].setup_irqs(&nonsec_irqs[i], 1, 1);
    }

  return true;
}
