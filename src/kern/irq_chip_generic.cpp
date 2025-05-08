INTERFACE:

#include "irq_chip.h"
#include "spin_lock.h"

class Irq_chip_gen : public Irq_chip_icu
{
public:
  Irq_chip_gen() = default;
  explicit Irq_chip_gen(unsigned nr_pins) { init(nr_pins); }

private:
  mutable Spin_lock<> _lock;
  unsigned _nr_pins = 0;
  Irq_base::Ptr *_pins = nullptr;
  Reserved_bitmap _reserved;
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
  _reserved.init(nr_pins);
}

PUBLIC inline
unsigned
Irq_chip_gen::nr_pins() const override
{ return _nr_pins; }

PUBLIC
Irq_base *
Irq_chip_gen::irq(Mword pin) const override
{
  auto guard = lock_guard(_lock);

  if (pin >= _nr_pins)
    return nullptr;

  if (_reserved[pin])
    return nullptr;

  return _pins[pin];
}

PUBLIC
bool
Irq_chip_gen::attach(Irq_base *irq, Mword pin, bool init = true) override
{
  {
    auto guard = lock_guard(_lock);

    if (pin >= _nr_pins)
      return false;

    if (_reserved[pin] || _pins[pin])
      return false;

    _pins[pin] = irq;
  }

  bind(irq, pin, !init);
  return true;
}

PUBLIC
void
Irq_chip_gen::detach(Irq_base *irq) override
{
  auto guard = lock_guard(_lock);

  Mword pin = irq->pin();

  mask(pin);
  Mem::barrier();
  Irq_chip_icu::detach(irq);
  _pins[pin] = nullptr;
}

PUBLIC
bool
Irq_chip_gen::reserve(Mword pin) override
{
  auto guard = lock_guard(_lock);

  if (pin >= _nr_pins)
    return false;

  if (_pins[pin] || _reserved[pin])
    return false;

  _reserved.set_bit(pin);
  return true;
}
