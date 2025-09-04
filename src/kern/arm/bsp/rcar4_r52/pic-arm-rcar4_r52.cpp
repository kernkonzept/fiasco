INTERFACE [arm && pic_gic && pf_rcar4_r52]:

#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_rcar4_r52]:

#include "boot_alloc.h"
#include "gic.h"
#include "gic_v3.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v3> M;

  void *dist_mmio = Kmem_mmio::map(Mem_layout::Gic_phys_base,
                                   Mem_layout::Gic_phys_size);
  void *redist_mmio = offset_cast<void *>(dist_mmio,
                                          Mem_layout::Gic_redist_offset);

  M *m = new Boot_object<M>(dist_mmio, redist_mmio);

  gic = &m->c;
  Irq_mgr::mgr = m;
}
