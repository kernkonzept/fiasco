INTERFACE:

#include "irq_chip_generic.h"
#include "mmio_register_block.h"

class Irq_chip_ingenic : public Irq_chip_gen
{
private:
  enum : Address
  {
    R_status     = 0x00,
    R_mask       = 0x04,
    R_set_mask   = 0x08,
    R_clear_mask = 0x0c,
    R_pending    = 0x10,
  };

  Register_block<32> _r;
};


IMPLEMENTATION:

PUBLIC inline
Irq_chip_ingenic::Irq_chip_ingenic(Address mmio)
: Irq_chip_gen(32), _r(mmio)
{
  _r[R_mask] = 0xffffffff;
}

PUBLIC inline
Unsigned32
Irq_chip_ingenic::pending() const
{
  return _r[R_pending];
}

PUBLIC inline NEEDS[Irq_chip_ingenic::pending]
bool
Irq_chip_ingenic::handle_pending(Upstream_irq const *ui)
{
  unsigned s = __builtin_ffs(pending());
  if (!s)
    return false;

  handle_irq<Irq_chip_ingenic>(s - 1, ui);
  return true;
}

PUBLIC
void
Irq_chip_ingenic::unmask(Mword pin) override
{
  _r[R_clear_mask] = 1UL << pin;
}

PUBLIC
void
Irq_chip_ingenic::mask(Mword pin) override
{
  _r[R_set_mask] = 1UL << pin;
}

PUBLIC
void
Irq_chip_ingenic::mask_and_ack(Mword pin) override
{
  _r[R_set_mask] = 1UL << pin;
}

PUBLIC
void
Irq_chip_ingenic::ack(Mword) override
{}

PUBLIC
int
Irq_chip_ingenic::set_mode(Mword, Mode) override
{
  return 0;
}

PUBLIC
bool
Irq_chip_ingenic::is_edge_triggered(Mword) const override
{ return false; }

PUBLIC inline
void
Irq_chip_ingenic::set_cpu(Mword, Cpu_number) override
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
char const *
Irq_chip_ingenic::chip_type() const override
{ return "Ingenic"; }

