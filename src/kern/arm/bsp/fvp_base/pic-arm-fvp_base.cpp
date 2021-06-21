INTERFACE [arm && pic_gic && pf_fvp_base]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_fvp_base]:

#include "irq_mgr_msi.h"
#include "irq_mgr_multi_chip.h"
#include "gic_v3.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  auto *g =
    new Boot_object<Gic_v3>(Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                             Gic_dist::Size),
                            Kmem::mmio_remap(Mem_layout::Gic_redist_phys_base,
                                             Mem_layout::Gic_redist_size));

  if (Gic_v3::Have_lpis)
    g->add_its(Kmem::mmio_remap(Mem_layout::Gic_its_phys_base,
                                Mem_layout::Gic_its_size));

  gic = g;
  Irq_mgr::mgr = new Boot_object<Irq_mgr_msi<Gic_v3, Gic_msi>>(g,
                                                               g->msi_chip());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_fvp_base]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
