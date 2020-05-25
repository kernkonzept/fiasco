INTERFACE [arm && pic_gic && pf_sunxi]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_sunxi]:

#include "gic_v2.h"
#include "irq_mgr_multi_chip.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<8> M;

  M *m = new Boot_object<M>(1);

  gic = new Boot_object<Gic_v2>(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base,
                                                 Gic_cpu_v2::Size),
                                Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                                 Gic_dist::Size));
  m->add_chip(0, gic, gic->nr_irqs());

  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_sunxi]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
