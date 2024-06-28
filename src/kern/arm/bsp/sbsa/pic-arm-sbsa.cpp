INTERFACE [arm && pic_gic && pf_sbsa]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_sbsa]:

#include "acpi.h"
#include "irq_mgr_msi.h"
#include "gic_v3.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_msi<Gic_v3, Gic_msi> M;

  auto *madt = Acpi::find<Acpi_madt const *>("APIC");
  if (!madt)
    panic("SBSA: no MADT found!\n");

  auto *dist = madt->find<Acpi_madt::Gic_distributor_if>(0);
  if (!dist)
    panic("SBSA: no distributor defined in MADT!\n");

  if (dist->version < Acpi_madt::Gic_distributor_if::V3)
    panic("SBSA: GICv3 or higher expected!\n");

  // Should we support more than one redistributor range?
  auto *redist = madt->find<Acpi_madt::Gic_redistributor_if>(0);
  if (!redist)
    panic("SBSA: no redistributor defined in MADT\n");

  auto *g =
    new Boot_object<Gic_v3>(Kmem_mmio::map(dist->base, Gic_dist::Size),
                            Kmem_mmio::map(redist->base, redist->length));

  int i = 0;
  while (auto *its = madt->find<Acpi_madt::Gic_its_if>(i++))
    g->add_its(Kmem_mmio::map(its->base, Mem_layout::Gic_its_size));

  gic = g;
  Irq_mgr::mgr = new Boot_object<M>(g, g->msi_chip());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_sbsa]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
