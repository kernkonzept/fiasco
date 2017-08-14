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
#include "cm.h"

static void gic_hit(Irq_base *_self, Upstream_irq const *u)
{
  Cascade_irq *self = nonull_static_cast<Cascade_irq *>(_self);
  Gic *i = nonull_static_cast<Gic *>(self->child());
  Upstream_irq ui(self, u);
  unsigned irq = i->pending();

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

  Address my_gic_base = 0x1bdc0000;
  Cm::cm->set_gic_base_and_enable(my_gic_base);

  auto *gic = new Boot_object<Gic>(Kmem::mmio_remap(my_gic_base), 2);
  auto *c = new Boot_object<Cascade_irq>(gic, gic_hit);
  Mips_cpu_irqs::chip->alloc(c, 2);
  c->unmask();
  m->add_chip(gic, 0); // expose GIC IRQs starting from IRQ 0

  m->print_infos();
}

PUBLIC static
void
Mips_bsp_irqs::init_ap(Cpu_number)
{
  Mips_cpu_irqs::chip->unmask(2); // GIC
}
