INTERFACE:

#include "types.h"

class Mips_bsp_irqs {};

IMPLEMENTATION:

#include "cm.h"
#include "i8259.h"
#include "gic.h"
#include "kmem.h"
#include "irq_mgr_flex.h"
#include "gt64120.h"
#include "boot_alloc.h"
#include "assert.h"
#include "cascade_irq.h"
#include "mips_cpu_irqs.h"
#include "mem_layout.h"

struct Mmio_io_adapter
{
  typedef Address Port_addr;
  static void out8(Unsigned8 v, Address a)
  { *reinterpret_cast<Unsigned8 volatile *>(a) = v; }

  static Unsigned8 in8(Address a)
  { return *reinterpret_cast<Unsigned8 volatile *>(a); }

  static void iodelay()
  {}
};

typedef Irq_chip_i8259_gen<Mmio_io_adapter> I8259;

static Gt64120 *syscon;

static void i8259_gt64120_hit(Irq_base *_self, Upstream_irq const *u)
{
  Cascade_irq *self = nonull_static_cast<Cascade_irq *>(_self);
  I8259 *i = nonull_static_cast<I8259*>(self->child());
  Upstream_irq ui(self, u);
  unsigned irq = syscon->gt_regs()[Gt64120::R::Pci0_iack] & 0x0f;
  i->handle_irq<I8259>(irq, &ui);
}

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

  syscon = new Boot_object<Gt64120>(Mem_layout::ioremap_nocache(0x1be00000, 0x1000),
                                    Mem_layout::ioremap_nocache(0x18000000, 0x1000));
  assert (syscon);

  typedef Irq_chip_i8259_gen<Mmio_io_adapter> I8259;
  auto *i8259 = new Boot_object<I8259>(
      syscon->pci_io()->get_mmio_base() + 0x20,
      syscon->pci_io()->get_mmio_base() + 0xa0);

  i8259->init(0);
  printf("GT64120: %p  i8259: %p\n", syscon, i8259);
  auto *m =  new Boot_object<Irq_mgr_flex<10> >();
  Irq_mgr::mgr = m;
  m->add_chip(i8259, 0);

  auto *pic_c = new Boot_object<Cascade_irq>(i8259, i8259_gt64120_hit);

  if (Cm::present())
    {
      Address my_gic_base = 0x1BDC0000;
      Cm::cm->set_gic_base_and_enable(my_gic_base);
      Gic *gic = new Boot_object<Gic>(Kmem::mmio_remap(my_gic_base), 4);
      auto *c = new Boot_object<Cascade_irq>(gic, gic_hit);
      Mips_cpu_irqs::chip->alloc(c, 4);
      c->unmask();
      m->add_chip(gic, 32); // expose GIC IRQs starting from IRQ 32

      gic->alloc(pic_c, 3);
    }
  else
    Mips_cpu_irqs::chip->alloc(pic_c, 2);

  pic_c->unmask();

  printf("IRQs: global IRQ assignments\n");
  m->print_infos();
}

PUBLIC static
void
Mips_bsp_irqs::init_ap(Cpu_number)
{
  Mips_cpu_irqs::chip->unmask(2); // i8259
  if (Cm::present())
    Mips_cpu_irqs::chip->unmask(4); // GIC
}
