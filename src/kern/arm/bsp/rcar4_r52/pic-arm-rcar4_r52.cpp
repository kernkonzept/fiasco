INTERFACE [arm && pic_gic && pf_rcar4_r52]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_rcar4_r52]:

#include "irq_mgr_multi_chip.h"
#include "gic_v3.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<10> M;

  auto regs = Kmem::mmio_remap(Mem_layout::Gic_phys_base,
                               Mem_layout::Gic_phys_size);

  *gic = new Boot_object<Gic_v3>(regs, regs + Mem_layout::Gic_redist_offset);

  M *m = new Boot_object<M>(1);
  m->add_chip(0, *gic, (*gic)->nr_irqs());

  *Irq_mgr::mgr = m;
}
