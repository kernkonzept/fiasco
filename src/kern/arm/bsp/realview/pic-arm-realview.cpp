INTERFACE [arm && realview]:

#include "initcalls.h"
#include "types.h"
#include "gic.h"

class Irq_base;

//-------------------------------------------------------------------
INTERFACE [arm && realview && (mpcore || armca9)]:

EXTENSION class Pic
{
private:
  enum
  {
    INTMODE_NEW_NO_DDC = 1 << 23,
  };
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && !(mpcore || armca9)]:

PRIVATE static inline
void Pic::configure_core()
{}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && realview && (realview_pb11mp || (realview_eb && (mpcore || (armca9 && mp))))]:

#include "irq_mgr_multi_chip.h"
#include "cascade_irq.h"

PUBLIC static
void Pic::init_ap(Cpu_number, bool resume)
{
  gic->init_ap(resume);
  static_cast<Gic*>(Irq_mgr::mgr->chip(256).chip)->init_ap(resume);
}


PUBLIC static FIASCO_INIT
void Pic::init()
{
  configure_core();
  typedef Irq_mgr_multi_chip<8> Mgr;

  Gic *g = gic.construct(Kmem::mmio_remap(Mem_layout::Gic_cpu_phys_base),
                         Kmem::mmio_remap(Mem_layout::Gic_dist_phys_base));
  Mgr *m = new Boot_object<Mgr>(2);
  Irq_mgr::mgr = m;

  m->add_chip(0, g, g->nr_irqs());

  g = new Boot_object<Gic>(Kmem::mmio_remap(Mem_layout::Gic1_cpu_phys_base),
                           Kmem::mmio_remap(Mem_layout::Gic1_dist_phys_base));
  m->add_chip(256, g, g->nr_irqs());

  // FIXME: Replace static local variable, use placement new
  Cascade_irq *casc_irq = new Boot_object<Cascade_irq>(g, &Gic::cascade_hit);

  gic->alloc(casc_irq, 42);
  casc_irq->unmask();
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && !(realview && (realview_pb11mp || (realview_eb && (mpcore || (armca9 && mp)))))]:

#include "irq_mgr_multi_chip.h"

PUBLIC static FIASCO_INIT
void Pic::init()
{
  configure_core();

  typedef Irq_mgr_multi_chip<8> Mgr;

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

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && (mpcore || armca9)]:

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
