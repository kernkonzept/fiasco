IMPLEMENTATION [arm && iommu && pf_sbsa]:

#include "kmem.h"
#include "acpi.h"

IMPLEMENT
bool
Iommu::init_platform()
{
  auto *iort = Acpi::find<Acpi_iort const *>("IORT");
  if (!iort)
    {
      WARNX(Error, "SBSA: no IORT found!\n");
      return false;
    }

  unsigned num = 0;
  for (auto const &node : *iort)
    {
      if (node->type == Acpi_iort::Node::Smmu_v2)
        panic("SBSA: SMMUv2 not supported!\n");

      if (node->type == Acpi_iort::Node::Smmu_v3)
        num++;
    }

  _iommus = Iommu_array(new Boot_object<Iommu>[num], num);

  unsigned i = 0;
  for (auto const &node : *iort)
    {
      if (node->type != Acpi_iort::Node::Smmu_v3)
        continue;

      auto const *smmu = static_cast<Acpi_iort::Smmu_v3 const *>(node);
      void *v = Kmem_mmio::map(smmu->base_addr, 0x100000);
      _iommus[i++].setup(v, smmu->gsiv_event, smmu->gsiv_gerr);
    }

  return true;
}
