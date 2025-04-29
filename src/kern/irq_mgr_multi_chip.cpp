INTERFACE:

#include "irq_mgr.h"

template< unsigned Bits_per_entry >
class Irq_mgr_multi_chip : public Irq_mgr
{
public:
  unsigned nr_irqs() const override { return _nchips * Irqs_per_entry; }
  unsigned nr_msis() const override { return 0; }

private:
  enum { Irqs_per_entry = 1UL << Bits_per_entry };

  struct Chip
  {
    unsigned irq_base;
    Irq_chip_icu *chip;
  };

  unsigned _nchips;
  Chip *_chips;
};

IMPLEMENTATION:

#include <cassert>
#include <cstring>

#include "boot_alloc.h"

PUBLIC explicit
template< unsigned Bits_per_chip >
Irq_mgr_multi_chip<Bits_per_chip>::Irq_mgr_multi_chip(unsigned chips)
  : _nchips(chips),
    _chips(new Boot_object<Chip>[chips]())
{}

PUBLIC template< unsigned Bits_per_entry >
Irq_mgr::Irq
Irq_mgr_multi_chip<Bits_per_entry>::chip(Mword irqnum) const override
{
  unsigned c = irqnum / Irqs_per_entry;
  if (c >= _nchips)
    return Irq();

  Chip *ci = _chips + c;

  return Irq(ci->chip, irqnum - ci->irq_base);
}

PUBLIC
template< unsigned Bits_per_entry >
void
Irq_mgr_multi_chip<Bits_per_entry>::add_chip(unsigned irq_base,
                                             Irq_chip_icu *c, unsigned pins)
{
  // check if the base is properly aligned
  assert((irq_base % Irqs_per_entry) == 0);

  unsigned idx = irq_base / Irqs_per_entry;
  unsigned num = (pins + Irqs_per_entry - 1) / Irqs_per_entry;

  assert (num);
  assert (idx < _nchips);
  assert (idx + num <= _nchips);

  for (unsigned i = idx; i < idx + num; ++i)
    {
      assert (!_chips[i].chip);
      _chips[i].chip = c;
      _chips[i].irq_base = irq_base;
    }
}
