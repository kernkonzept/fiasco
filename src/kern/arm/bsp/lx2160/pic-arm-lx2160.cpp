INTERFACE [arm && pic_gic && pf_lx2160]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_lx2160]:

#include "boot_alloc.h"
#include "gic_v3.h"
#include "irq_mgr_msi.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  auto *g =
    new Boot_object<Gic_v3>(Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size),
                            Kmem_mmio::map(Mem_layout::Gic_redist_phys_base,
                                           Mem_layout::Gic_redist_size));

  g->add_its(Kmem_mmio::map(Mem_layout::Gic_its_phys_base,
                            Mem_layout::Gic_its_size));

  gic = g;

  typedef Irq_mgr_msi<Gic_v3, Gic_msi> Mgr;
  Irq_mgr::mgr = new Boot_object<Mgr>(g, g->msi_chip());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_lx2160]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
