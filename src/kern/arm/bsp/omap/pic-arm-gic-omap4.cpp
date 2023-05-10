INTERFACE [arm && pic_gic && (pf_omap4 || pf_omap5)]:

#include "initcalls.h"
#include "gic.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && (pf_omap4 || pf_omap5)]:

#include "boot_alloc.h"
#include "gic_v2.h"
#include "irq_mgr.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v2> M;

  M *m = new Boot_object<M>(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base,
                                             Gic_cpu_v2::Size),
                            Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                             Gic_dist::Size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pic_gic && (pf_omap4 || pf_omap5)]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
