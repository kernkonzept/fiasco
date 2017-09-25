INTERFACE:

#include "types.h"

class Mips_bsp_irqs {};

IMPLEMENTATION:

#include "irq_mgr_flex.h"
#include "boot_alloc.h"
#include "assert.h"
#include "cascade_irq.h"
#include "mips_cpu_irqs.h"
#include "gic.h"
#include "kmem.h"

static Gic *gic;

static void gic_hit(Irq_base *_self, Upstream_irq const *u)
{
  Cascade_irq *self = nonull_static_cast<Cascade_irq *>(_self);
  Gic *i = nonull_static_cast<Gic *>(self->child());

  Upstream_irq ui(self, u);
  unsigned irq = gic->pending();
  if (EXPECT_TRUE(irq != ~0u))
    i->handle_irq<Gic>(irq, &ui);
}

PUBLIC static
void
Mips_bsp_irqs::init(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  auto *m =  new Boot_object<Irq_mgr_flex<10> >();
  Irq_mgr::mgr = m;

  m->add_chip(Mips_cpu_irqs::chip, 0);

  Register_block<32> sead3_cfg_r(Kmem::mmio_remap(0x1b100110));
  enum { GIC_PRESENT = 1 << 1 };
  if (sead3_cfg_r[0] & GIC_PRESENT)
    {
      gic = new Boot_object<Gic>(Kmem::mmio_remap(0x1b1c0000), 2);
      auto *c = new Boot_object<Cascade_irq>(gic, gic_hit);
      Mips_cpu_irqs::chip->alloc(c, 2);
      c->unmask();
      m->add_chip(gic, 8);
    }

  m->print_infos();
}

PUBLIC static
void
Mips_bsp_irqs::init_ap(Cpu_number)
{
  // FIXME: missing MP support
}

