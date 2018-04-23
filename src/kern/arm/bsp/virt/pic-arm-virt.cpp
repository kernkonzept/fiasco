INTERFACE [arm && pf_arm_virt]:

#include "initcalls.h"
#include "types.h"
#include "gic.h"

class Irq_base;

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pf_arm_virt]:

#include "irq_mgr_multi_chip.h"

PUBLIC static FIASCO_INIT
void Pic::init()
{
  typedef Irq_mgr_multi_chip<9> Mgr;

  Gic *g = gic.construct(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base),
                         Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base));

  Mgr *m = new Boot_object<Mgr>(1);
  m->add_chip(0, g, g->nr_irqs());
  Irq_mgr::mgr = m;
}

PUBLIC static
void Pic::init_ap(Cpu_number, bool resume)
{
  gic->init_ap(resume);
}
