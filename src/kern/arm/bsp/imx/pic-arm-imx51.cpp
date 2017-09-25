INTERFACE [arm && pic_gic && (imx51 || imx53 || imx6)]:

#include "initcalls.h"
#include "gic.h"

INTERFACE [arm && pic_gic && (imx51 | imx53)]:

EXTENSION class Pic
{
  enum { Gic_sz = 7 };
};

INTERFACE [arm && pic_gic && imx6]:

EXTENSION class Pic
{
  enum { Gic_sz = 8 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && (imx51 || imx53 || imx6)]:

#include "irq_mgr_multi_chip.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<Gic_sz> M;

  M *m = new Boot_object<M>(1);

  gic.construct(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base),
                Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base));
  m->add_chip(0, gic, gic->nr_irqs());

  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && imx6]:

PUBLIC static
void Pic::init_ap(Cpu_number, bool resume)
{
  gic->init_ap(resume);
}
