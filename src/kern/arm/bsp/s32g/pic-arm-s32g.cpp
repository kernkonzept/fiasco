INTERFACE [arm && pic_gic && pf_s32g]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_s32g]:

#include "boot_alloc.h"
#include "irq_mgr.h"
#include "gic_v3.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v3> M;

  M *m = new Boot_object<M>(Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size),
                            Kmem_mmio::map(Mem_layout::Gic_redist_phys_base,
                                           Mem_layout::Gic_redist_size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_s32g]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
