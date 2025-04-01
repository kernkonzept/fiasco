INTERFACE:

#include "irq_chip.h"

class Irq_chip_gen : public Irq_chip_icu
{
public:
  Irq_chip_gen() = default;
  explicit Irq_chip_gen(unsigned nr_pins) { init(nr_pins); }

private:
  unsigned _nr_pins = 0;
  Irq_base::Ptr *_pins = nullptr;
};

// -------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstring>
#include "boot_alloc.h"
#include "mem.h"

PUBLIC
void
Irq_chip_gen::init(unsigned nr_pins)
{
  _nr_pins = nr_pins;
  _pins = Boot_alloced::allocate<Irq_base *>(nr_pins);
  memset(_pins, 0, sizeof(Irq_base*) * nr_pins);
}

PUBLIC inline
unsigned
Irq_chip_gen::nr_pins() const override
{ return _nr_pins; }

PUBLIC
Irq_base *
Irq_chip_gen::irq(Mword pin) const override
{
  if (pin >= _nr_pins)
    return nullptr;

  return _pins[pin];
}

PUBLIC
bool
Irq_chip_gen::attach(Irq_base *irq, Mword pin, bool init = true) override
{
  if (pin >= _nr_pins)
    return false;

  if (_pins[pin])
    return false;

  _pins[pin] = irq;
  bind(irq, pin, !init);
  return true;
}

PUBLIC
void
Irq_chip_gen::detach(Irq_base *irq) override
{
  mask(irq->pin());
  Mem::barrier();
  _pins[irq->pin()] = nullptr;
  Irq_chip_icu::detach(irq);
}

PUBLIC
bool
Irq_chip_gen::reserve(Mword pin) override
{
  if (pin >= _nr_pins)
    return false;

  if (_pins[pin])
    return false;

  _pins[pin] = reinterpret_cast<Irq_base*>(1);
  return true;
}
