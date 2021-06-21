INTERFACE:

#include "gic_its.h"
#include "irq_chip_generic.h"

class Gic_msi : public Irq_chip_gen
{
  Gic_its *_its;
};

IMPLEMENTATION:

PUBLIC inline
void
Gic_msi::init(Gic_its *its, unsigned nrmsis)
{
  _its = its;
  Irq_chip_gen::init(nrmsis);
}

PUBLIC
int
Gic_msi::set_mode(Mword, Mode) override
{ return 0; }

PUBLIC
bool
Gic_msi::is_edge_triggered(Mword) const override
{ return true; }

PUBLIC
void
Gic_msi::mask(Mword pin) override
{
  _its->mask_lpi(pin);
}

PUBLIC
void
Gic_msi::ack(Mword pin) override
{
  _its->ack_lpi(pin);
}

PUBLIC
void
Gic_msi::mask_and_ack(Mword pin) override
{
  assert (cpu_lock.test());
  mask(pin);
  ack(pin);
}

PUBLIC
void
Gic_msi::unmask(Mword pin) override
{
  _its->unmask_lpi(pin);
}

PUBLIC
void
Gic_msi::set_cpu(Mword pin, Cpu_number cpu) override
{
  _its->assign_lpi_to_cpu(pin, cpu);
}

PUBLIC
void
Gic_msi::unbind(Irq_base *irq) override
{
  _its->free_lpi(irq->pin());
  Irq_chip_gen::unbind(irq);
}

PUBLIC
int
Gic_msi::msg(Mword pin, Unsigned64 src, Irq_mgr::Msi_info *inf)
{
  if (pin >= nr_irqs())
    return -L4_err::ERange;

  return _its->bind_lpi_to_device(pin, src, inf);
}

IMPLEMENTATION [debug]:

PUBLIC inline
char const *
Gic_msi::chip_type() const override
{ return "GIC-MSI"; }
