INTERFACE [arm && pic_gic && pf_armada38x]:

#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_armada38x]:

#include "boot_alloc.h"
#include "gic.h"
#include "gic_v2.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v2> M;

  M *m = new Boot_object<M>(Kmem_mmio::map(Mem_layout::Gic_cpu_phys_base,
                                           Gic_cpu_v2::Size),
                            Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}
