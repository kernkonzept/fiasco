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

  unsigned nr_pins() const override
  { return cxx::size(_irqs); }

  Irq_base *irq(Mword pin) const override
  {
    if (pin >= Mips_cpu_irq_chip::nr_pins())
      return nullptr;

    return _irqs[pin];
  }

  bool attach(Irq_base *irq, Mword pin, bool init = true) override
  {
    if (pin >= Mips_cpu_irq_chip::nr_pins())
      return false;

    if (_irqs[pin])
      return false;

    if (!cas<Irq_base *>(&_irqs[pin], nullptr, irq))
      return false;

    bind(irq, pin, !init);
    return true;
  }

  void detach(Irq_base *irq) override
  {
    mask(irq->pin());
    Mem::barrier();
    _irqs[irq->pin()] = 0;
    Irq_chip_icu::detach(irq);
  }

  bool reserve(Mword pin) override
  {
    if (pin >= Mips_cpu_irq_chip::nr_pins())
      return false;

    if (_irqs[pin])
      return false;

    _irqs[pin] = reinterpret_cast<Irq_base*>(1);

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

  void mask_percpu(Cpu_number, Mword pin) override
  {
    mask(pin);
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

  bool set_cpu(Mword, Cpu_number) override
  {
    // All our IRQs are CPU-local IRQs, so nothing to do
    return false;
  }

  void unmask(Mword pin) override
  {
    Mword s = Cp0_status::read();
    s |= 0x100UL << pin;
    Cp0_status::write(s);
    // NOTE: we do not use 'ehb' here as this is done with IRQs disabled
    // and enabling IRQs must use 'ehb'
  }

  void unmask_percpu(Cpu_number, Mword pin) override
  {
    unmask(pin);
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

