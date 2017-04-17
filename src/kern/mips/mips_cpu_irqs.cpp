/*
 * MIPS top-level IRQ handling: provide a first stage IRQ chip with 8 slots for
 * Irq_base object that either signal IRQs to second level IRQ chips or to the
 * user.
 */
INTERFACE:

class Irq_chip_icu;

class Mips_cpu_irqs
{
public:
  static Irq_chip_icu *chip;
};

IMPLEMENTATION:

#include "atomic.h"
#include "irq_chip.h"
#include "cp0_status.h"
#include "mem.h"

class Mips_cpu_irq_chip : public Irq_chip_icu
{
public:
  Mips_cpu_irq_chip()
  {
    for (auto &i: _irqs)
      i = 0;
  }

  unsigned nr_irqs() const override
  { return sizeof(_irqs) / sizeof(_irqs[0]); }

  Irq_base *irq(Mword pin) const override
  { return _irqs[pin]; }

  bool alloc(Irq_base *irq, Mword pin) override
  {
    if (pin >= Mips_cpu_irq_chip::nr_irqs())
      return false;

    if (_irqs[pin])
      return false;

    if (!mp_cas(&_irqs[pin], (Irq_base *)0, irq))
      return false;

    bind(irq, pin);
    return true;
  }

  void unbind(Irq_base *irq) override
  {
    mask(irq->pin());
    Mem::barrier();
    _irqs[irq->pin()] = 0;
    Irq_chip_icu::unbind(irq);
  }

  bool reserve(Mword pin) override
  {
    if (pin >= Mips_cpu_irq_chip::nr_irqs())
      return false;

    if (_irqs[pin])
      return false;

    _irqs[pin] = (Irq_base*)1;

    return true;
  }

  void mask(Mword pin) override
  {
    Mword s = Cp0_status::read();
    s &= ~(0x100UL << pin);
    Cp0_status::write(s);
    // NOTE: we do not use 'ehb' here as this is done with IRQs disabled
    // and enabling IRQs must use 'ehb'
  }

  void ack(Mword) override
  {}

  void mask_and_ack(Mword pin) override
  {
    Mips_cpu_irq_chip::mask(pin);
  }

  int set_mode(Mword, Mode) override
  {
    return Mode::Trigger_edge | Mode::Polarity_high;
  }

  bool is_edge_triggered(Mword) const override
  { return true; }

  void set_cpu(Mword, Cpu_number) override
  {
    // All our IRQs are CPU-local IRQs, so nothing to do
  }

  void unmask(Mword pin) override
  {
    Mword s = Cp0_status::read();
    s |= 0x100UL << pin;
    Cp0_status::write(s);
    // NOTE: we do not use 'ehb' here as this is done with IRQs disabled
    // and enabling IRQs must use 'ehb'
  }

private:
  Irq_base *_irqs[8];
};

Irq_chip_icu *Mips_cpu_irqs::chip;

static Static_object<Mips_cpu_irq_chip> _chip;

PUBLIC static
void
Mips_cpu_irqs::init()
{
  chip = _chip.construct();
}

extern "C" void
handle_irq(unsigned long irq)
{
  _chip->handle_irq<Mips_cpu_irq_chip>(irq, 0);
}


IMPLEMENTATION [debug]:

PUBLIC char const *
Mips_cpu_irq_chip::chip_type() const override
{ return "MIPScpu"; }

