INTERFACE [arm && pic_gic && pf_imx]:

#include "initcalls.h"
#include "gic.h"

// ------------------------------------------------------------------------
INTERFACE [arm && pic_gic && (pf_imx_51 || pf_imx_53)]:

EXTENSION class Pic
{
  enum { Gic_sz = 7 };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pic_gic && (pf_imx_6 || pf_imx_6ul || pf_imx_7 || pf_imx_8m)]:

EXTENSION class Pic
{
  enum { Gic_sz = 8 };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pic_gic && pf_imx_8xq]:

EXTENSION class Pic
{
  enum { Gic_sz = 10 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_imx && have_arm_gicv2]:

#include "gic_v2.h"
#include "irq_mgr_multi_chip.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<Gic_sz> M;

  M *m = new Boot_object<M>(1);

  gic = new Boot_object<Gic_v2>(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base,
                                                 Gic_cpu_v2::Size),
                                Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                                 Gic_dist::Size));
  m->add_chip(0, gic, gic->nr_irqs());

  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_imx && have_arm_gicv3]:

#include "irq_mgr_multi_chip.h"
#include "kmem.h"
#include "gic_v3.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<Gic_sz> M;

  M *m = new Boot_object<M>(1);

  Mmio_register_block dist(Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                            Gic_dist::Size));
  gic = new Boot_object<Gic_v3>(dist.get_mmio_base(),
                                Kmem::mmio_remap(Mem_layout::Gic_redist_phys_base,
                                                 Mem_layout::Gic_redist_phys_size));
  m->add_chip(0, gic, gic->nr_irqs());

  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && (pf_imx_6 || pf_imx_7 || arm_v8)]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
