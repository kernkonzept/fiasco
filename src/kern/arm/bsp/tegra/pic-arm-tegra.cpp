INTERFACE [arm && pic_gic && pf_tegra]:

#include "gic.h"
#include "initcalls.h"

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_tegra && (pf_tegra_tegra2 || pf_tegra_tegra3)]:

#include "boot_alloc.h"
#include "gic_v2.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "gic.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v2> M;

  M *m = new Boot_object<M>(Kmem_mmio::map(Mem_layout::Gic_cpu_phys_base,
                                           Gic_cpu_v2::Size),
                            Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_tegra && pf_tegra_orin]:

#include "boot_alloc.h"
#include "gic_v3.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "gic.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v3> M;

  M *m = new Boot_object<M>(Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size),
                            Kmem_mmio::map(Mem_layout::Gic_redist_phys_base,
                                           Mem_layout::Gic_redist_size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pic_gic && pf_tegra]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
