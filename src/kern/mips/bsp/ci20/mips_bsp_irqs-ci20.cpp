INTERFACE:

#include "types.h"

class Mips_bsp_irqs {};

IMPLEMENTATION:

#include "assert.h"
#include "irq_ingenic.h"
#include "irq_mgr_flex.h"
#include "boot_alloc.h"
#include "cascade_irq.h"
#include "mips_cpu_irqs.h"

static Irq_chip_ingenic *_ic[2];

static void ingenic_cascade(Irq_base *_self, Upstream_irq const *u)
{
  Cascade_irq *self = nonull_static_cast<Cascade_irq *>(_self);
  Upstream_irq ui(self, u);
  for (auto c: _ic)
    if (c->handle_pending(&ui))
      return;
}

PUBLIC static
void
Mips_bsp_irqs::init(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  auto *m =  new Boot_object<Irq_mgr_flex<10> >();
  Irq_mgr::mgr = m;

  _ic[0] = new Boot_object<Irq_chip_ingenic>(0xb0001000);
  m->add_chip(_ic[0], 0);
  _ic[1] = new Boot_object<Irq_chip_ingenic>(0xb0001020);
  m->add_chip(_ic[1], 32);

  auto *c = new Boot_object<Cascade_irq>(nullptr, ingenic_cascade);
  Mips_cpu_irqs::chip->alloc(c, 2);
  c->unmask();
  printf("IRQs: global IRQ assignments\n");
  m->print_infos();
}

PUBLIC static
void
Mips_bsp_irqs::init_ap(Cpu_number)
{}
