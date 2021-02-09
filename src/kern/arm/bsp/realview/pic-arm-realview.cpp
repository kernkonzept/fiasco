INTERFACE [arm && pf_realview]:

#include "initcalls.h"
#include "types.h"
#include "gic.h"

class Irq_base;

//-------------------------------------------------------------------
INTERFACE [arm && pf_realview && (arm_mpcore || arm_cortex_a9)]:

EXTENSION class Pic
{
private:
  enum
  {
    INTMODE_NEW_NO_DDC = 1 << 23,
  };
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && !(arm_mpcore || arm_cortex_a9)]:

PRIVATE static inline
void Pic::configure_core()
{}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic
                && (pf_realview_pb11mp
                    || (pf_realview_eb
                        && (arm_mpcore || (arm_cortex_a9 && mp))))]:

#include "gic_v2.h"
#include "irq_mgr_multi_chip.h"
#include "cascade_irq.h"

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
  static_cast<Gic*>(Irq_mgr::mgr->chip(256).chip)->init_ap(cpu, resume);
}


PUBLIC static FIASCO_INIT
void Pic::init()
{
  configure_core();
  typedef Irq_mgr_multi_chip<8> Mgr;

  Gic *g = new Boot_object<Gic_v2>(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base,
                                                    Gic_cpu_v2::Size),
                                   Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                                    Gic_dist::Size));
  gic = g;

  Mgr *m = new Boot_object<Mgr>(2);
  Irq_mgr::mgr = m;

  m->add_chip(0, g, g->nr_irqs());

  g = new Boot_object<Gic_v2>(Kmem::mmio_remap(Mem_layout::Gic1_cpu_phys_base,
                                               Gic_cpu_v2::Size),
                              Kmem::mmio_remap(Mem_layout::Gic1_dist_phys_base,
                                               Gic_dist::Size));
  m->add_chip(256, g, g->nr_irqs());

  Cascade_irq *casc_irq = new Boot_object<Cascade_irq>(g, &Gic_v2::cascade_hit);

  gic->alloc(casc_irq, 42);
  casc_irq->unmask();
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic
                && !(pf_realview_pb11mp
                     || (pf_realview_eb
                         && (arm_mpcore || (arm_cortex_a9 && mp))))]:

#include "gic_v2.h"
#include "irq_mgr_multi_chip.h"

PUBLIC static FIASCO_INIT
void Pic::init()
{
  configure_core();

  typedef Irq_mgr_multi_chip<8> Mgr;

  Gic *g = new Boot_object<Gic_v2>(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base,
                                                    Gic_cpu_v2::Size),
                                   Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base,
                                                    Gic_dist::Size));
  gic = g;

  Mgr *m = new Boot_object<Mgr>(1);
  m->add_chip(0, g, g->nr_irqs());
  Irq_mgr::mgr = m;
}

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && (arm_mpcore || arm_cortex_a9)]:

#include "cpu.h"
#include "io.h"
#include "platform.h"

PRIVATE static
void Pic::unlock_config()
{ Platform::sys->write<Mword>(0xa05f, Platform::Sys::Lock); }

PRIVATE static
void Pic::lock_config()
{ Platform::sys->write<Mword>(0x0, Platform::Sys::Lock); }

PRIVATE static
void Pic::configure_core()
{
  // Enable 'new' interrupt-mode, no DCC
  unlock_config();
  Platform::sys->modify<Mword>(INTMODE_NEW_NO_DDC, 0, Platform::Sys::Pld_ctrl1);
  lock_config();
}
