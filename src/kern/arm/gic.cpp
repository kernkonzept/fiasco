INTERFACE [arm && pic_gic]:

#include "kmem.h"
#include "irq_chip_generic.h"
#include "mmio_register_block.h"
#include "gic_cpu.h"
#include "gic_dist.h"


class Gic : public Irq_chip_gen
{
private:
  Gic_cpu _cpu;
  Gic_dist _dist;

public:
  enum
  {
    Cpu_prio_val      = Gic_cpu::Cpu_prio_val,
  };
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

#include <cassert>
#include <cstring>
#include <cstdio>

#include "cascade_irq.h"
#include "io.h"
#include "irq_chip_generic.h"
#include "panic.h"
#include "processor.h"

PUBLIC inline
unsigned
Gic::hw_nr_irqs()
{ return _dist.hw_nr_irqs(); }

PUBLIC inline
Unsigned32 Gic::pcpu_to_sgi(Cpu_phys_id cpu)
{ return Gic_dist::pcpu_to_sgi(cpu); }

PUBLIC inline
void Gic::softint_cpu(unsigned callmap, unsigned m)
{ _dist.softint_cpu(callmap, m); }

PUBLIC inline
void Gic::softint_bcast(unsigned m)
{ _dist.softint_bcast(m); }

PRIVATE
void
Gic::cpu_init(bool resume)
{
  _cpu.disable();

  if (!resume)
    _dist.cpu_init();

  _cpu.enable();

  // Ensure BSPs have provided a mapping for the CPUTargetList
  assert(pcpu_to_sgi(Proc::cpu_id()) < (1u << 8));
}

PUBLIC
void
Gic::init_ap(Cpu_number, bool resume)
{
  cpu_init(resume);
}

PUBLIC
unsigned
Gic::init(bool primary_gic, int nr_irqs_override = -1)
{
  _cpu.disable();
  unsigned num = _dist.init(primary_gic, Cpu_prio_val,
                            nr_irqs_override);
  _cpu.enable();
  return num;
}

PUBLIC
Gic::Gic(Address cpu_base, Address dist_base, int nr_irqs_override = -1)
  : _cpu(cpu_base), _dist(dist_base)
{
  unsigned num = init(true, nr_irqs_override);

  printf("Number of IRQs available at this GIC: %d\n", num);

  Irq_chip_gen::init(num);
}

/**
 * \brief Create a GIC device that is a physical alias for the
 *        master GIC.
 */
PUBLIC inline
Gic::Gic(Address cpu_base, Address dist_base, Gic *master_mapping)
  : _cpu(cpu_base), _dist(dist_base)
{
  Irq_chip_gen::init(master_mapping->nr_irqs());
}

PUBLIC inline
void Gic::disable_locked( unsigned irq )
{ _dist.disable_irq(irq); }

PUBLIC inline
void Gic::enable_locked(unsigned irq, unsigned /*prio*/)
{ _dist.enable_irq(irq); }

PUBLIC inline
void Gic::acknowledge_locked(unsigned irq)
{
  if (!Gic_dist::Config_mxc_tzic)
    _cpu.ack(irq);
}

PUBLIC
void
Gic::mask(Mword pin) override
{
  assert (cpu_lock.test());
  disable_locked(pin);
}

PUBLIC
void
Gic::mask_and_ack(Mword pin) override
{
  assert (cpu_lock.test());
  disable_locked(pin);
  acknowledge_locked(pin);
}

PUBLIC
void
Gic::ack(Mword pin) override
{
  acknowledge_locked(pin);
}


PUBLIC
void
Gic::unmask(Mword pin) override
{
  assert (cpu_lock.test());
  enable_locked(pin, 0xa);
}

PUBLIC
int
Gic::set_mode(Mword pin, Mode m) override
{
  return _dist.set_mode(pin, m);
}

PUBLIC
bool
Gic::is_edge_triggered(Mword pin) const override
{
  return _dist.is_edge_triggered(pin);
}

PUBLIC inline
void
Gic::hit(Upstream_irq const *u)
{
  Unsigned32 num = pending();
  if (EXPECT_FALSE(num == 0x3ff))
    return;

  handle_irq<Gic>(num, u);
}

PUBLIC static
void
Gic::cascade_hit(Irq_base *_self, Upstream_irq const *u)
{
  // this function calls some virtual functions that might be
  // ironed out
  Cascade_irq *self = nonull_static_cast<Cascade_irq*>(_self);
  Gic *gic = nonull_static_cast<Gic*>(self->child());
  Upstream_irq ui(self, u);
  gic->hit(&ui);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_em_tz]:

PUBLIC
bool
Gic::alloc(Irq_base *irq, Mword pin, bool init = true) override
{
  // allow local irqs to be allocated on each CPU
  return (pin < 32 && irq->chip() == this && irq->pin() == pin)
         || Irq_chip_gen::alloc(irq, pin, init);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz]:

PUBLIC
bool
Gic::alloc(Irq_base *irq, Mword pin, bool init = true)
{
  if ((pin < 32 && irq->chip() == this && irq->pin() == pin)
      || Irq_chip_gen::alloc(irq, pin, init))
    {
      printf("GIC: Switching IRQ %ld to secure\n", pin);
      _dist.setup_pin(pin);
      return true;
    }
  return false;
}

PUBLIC
void
Gic::set_pending_irq(unsigned idx, Unsigned32 val)
{
  _dist.set_pending_irq(idx, val);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && !mp && pic_gic]:

PUBLIC
void
Gic::set_cpu(Mword, Cpu_number) override
{}

PUBLIC inline
Unsigned32 Gic::pending()
{
  if (Gic_dist::Config_mxc_tzic)
    return _dist.mxc_pending();

  return _cpu.iar() & 0x3ff;
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pic_gic]:

#include "cpu.h"

PUBLIC inline NEEDS["io.h"]
Unsigned32 Gic::pending()
{
  Unsigned32 ack = _cpu.iar();

  // IPIs/SGIs need to take the whole ack value
  if ((ack & 0x3ff) < 16)
    _cpu.ack(ack);

  return ack & 0x3ff;
}

PUBLIC inline NEEDS["cpu.h"]
void
Gic::set_cpu(Mword pin, Cpu_number cpu) override
{
  _dist.set_cpu(pin, Cpu::cpus.cpu(cpu).phys_id());
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Gic::irq_prio(unsigned irq, unsigned prio)
{
  _dist.irq_prio(irq, prio);
}

PUBLIC
unsigned
Gic::irq_prio(unsigned irq)
{
  return _dist.irq_prio(irq);
}

PUBLIC
unsigned
Gic::pmr()
{ return _cpu.pmr(); }

PUBLIC
void
Gic::pmr(unsigned prio)
{ return _cpu.pmr(prio); }

PUBLIC
char const *
Gic::chip_type() const override
{ return "GIC"; }
