INTERFACE [arm && pic_gic && pf_rcar4]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_rcar4]:

#include "boot_alloc.h"
#include "gic_v3.h"
#include "irq_mgr.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v3> M;

  M *m = new Boot_object<M>(Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                             Gic_dist::Size),
                            Kmem::mmio_remap(Mem_layout::Gic_redist_phys_base,
                                             Mem_layout::Gic_redist_size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_rcar4]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
