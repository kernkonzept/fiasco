INTERFACE:

#include "irq_mgr.h"

template< unsigned Bits_per_entry >
class Irq_mgr_multi_chip : public Irq_mgr
{
public:
  unsigned nr_irqs() const { return _nchips << Bits_per_entry; }
  unsigned nr_msis() const { return 0; }

private:
  struct Chip
  {
    unsigned mask;
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
Irq_mgr_multi_chip<Bits_per_entry>::chip(Mword irqnum) const
{
  unsigned c = irqnum >> Bits_per_entry;
  if (c >= _nchips)
    return Irq();

  Chip *ci = _chips + c;

  return Irq(ci->chip, irqnum & ci->mask);
}


PUBLIC
template< unsigned Bits_per_entry >
void
Irq_mgr_multi_chip<Bits_per_entry>::add_chip(unsigned irq_base,
                                             Irq_chip_icu *c, unsigned pins)
{
  // check if the base is properly aligned
  assert ((irq_base & ~(~0UL << Bits_per_entry)) == 0);

  unsigned idx = irq_base >> Bits_per_entry;
  unsigned num = (pins + (1UL << Bits_per_entry) - 1) >> Bits_per_entry;

  unsigned mask = ~0U;
  while (mask & (pins - 1))
    mask <<= 1;

  assert (mask);
  mask = ~mask;

  // base irq must be aligned according to the number of pins
  assert (!(irq_base & mask));

  assert (num);
  assert (idx < _nchips);
  assert (idx + num <= _nchips);

  for (unsigned i = idx; i < idx + num; ++i)
    {
      assert (!_chips[i].chip);
      _chips[i].chip = c;
      _chips[i].mask = mask;
    }
}
