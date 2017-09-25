INTERFACE:

#include "irq_chip.h"


class Irq_chip_gen : public Irq_chip_icu
{
public:
  Irq_chip_gen() {}
  explicit Irq_chip_gen(unsigned nirqs) { init(nirqs); }

private:
  unsigned _nirqs;
  Irq_base **_irqs;
};


// -------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstring>

#include "boot_alloc.h"
#include "mem.h"

PUBLIC
void
Irq_chip_gen::init(unsigned nirqs)
{
  _nirqs = nirqs;
  _irqs = (Irq_base **)Boot_alloced::alloc(sizeof(Irq_base*) * nirqs);
  memset(_irqs, 0, sizeof(Irq_base*) * nirqs);
}

PUBLIC inline
unsigned
Irq_chip_gen::nr_irqs() const
{ return _nirqs; }

PUBLIC
Irq_base *
Irq_chip_gen::irq(Mword pin) const
{
  if (pin >= _nirqs)
    return 0;

  return _irqs[pin];
}

PUBLIC
bool
Irq_chip_gen::alloc(Irq_base *irq, Mword pin)
{
  if (pin >= _nirqs)
    return false;

  if (_irqs[pin])
    return false;

  _irqs[pin] = irq;
  bind(irq, pin);
  return true;
}

PUBLIC
void
Irq_chip_gen::unbind(Irq_base *irq)
{
  mask(irq->pin());
  Mem::barrier();
  _irqs[irq->pin()] = 0;
  Irq_chip_icu::unbind(irq);
}

PUBLIC
bool
Irq_chip_gen::reserve(Mword pin)
{
  if (pin >= _nirqs)
    return false;

  if (_irqs[pin])
    return false;

  _irqs[pin] = (Irq_base*)1;

  return true;
}
