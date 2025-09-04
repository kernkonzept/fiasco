INTERFACE [arm && pic_gic && pf_sbsa]:

#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_sbsa]:

#include "acpi.h"
#include "gic.h"
#include "gic_v3.h"
#include "gic_redist.h"
#include "irq_mgr_msi.h"
#include "kmem_mmio.h"

class Gic_redist_find_acpi : public Gic_redist_find
{
public:
  Gic_redist_find_acpi(const Acpi_madt *madt)
  : _madt(madt)
  {}

  Mmio_register_block get_redist_mmio(Unsigned64 mpid) override
  {
    bool region = false;
    for (auto const *redist : _madt->iterate<Acpi_madt::Gic_redistributor_if>())
      {
        region = true;
        // Assumes Kmem_mmio::map is smart and reuses existing mappings
        auto *r = Kmem_mmio::map(redist->base, redist->length);
        if (!r)
          return Mmio_register_block();
        auto b = scan_range(r, mpid);
        if (b.valid())
          return b;
      }

    // if there was a Acpi_madt::Gic_redistributor_if, other options do not
    // exist per ACPI spec
    if (region)
      return Mmio_register_block();

    Unsigned64 mpid_pure = mpid & 0xff00ffffff;
    // Think: The loop gets longer and longer the more cores we boot.
    // Assuming the ACPI table is MPID ordered (which often seems the case)
    // we could remember the loop iterator and continue there...
    for (auto const *gicc : _madt->iterate<Acpi_madt::Gic_cpu_if>())
      {
        if ((gicc->flags & Acpi_madt::Gic_cpu_if::Enabled)
            && gicc->mpidr == mpid_pure)
          {
            /* The size is just the maximum that could be there, to know better
             * we'd need to map it to check Gic_redist::GICR_TYPER.vlpis and
             * then map again with the potentially bigger size */
            if (auto *r = Kmem_mmio::map(gicc->gicr_base,
                                         Gic_redist::GICR_frame_size * 4))
              return Mmio_register_block(r);

            break;
          }
      }

    return Mmio_register_block();
  }

private:
  const Acpi_madt *_madt;
};

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

  auto *redist_find = new Boot_object<Gic_redist_find_acpi>(madt);

  auto *g =
    new Boot_object<Gic_v3>(Kmem_mmio::map(dist->base, Gic_dist::Size),
                            redist_find);

  int i = 0;
  while (auto *its = madt->find<Acpi_madt::Gic_its_if>(i++))
    g->add_its(Kmem_mmio::map(its->base, Mem_layout::Gic_its_size));

  gic = g;
  Irq_mgr::mgr = new Boot_object<M>(g, g->msi_chip());
}
