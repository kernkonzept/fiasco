INTERFACE:

#include "cpu.h"
#include "irq_chip_generic.h"
#include "kip.h"
#include "mmio_register_block.h"

#include <cxx/bitfield>

/**
  * Driver for the MiG-V PLIC.
  */
class Irq_chip_migv : public Irq_chip_gen
{
private:
  enum
  {
    Num_irqs    = 24,
    Num_targets = 2,
  };

  // 0 -> machine mode external interrupt
  // 1 -> supervisor mode external interrupt
  enum : unsigned
  {
    Config0       = 0x0,
    Config1       = 0x4,
    Trigger_mode  = 0x8,
    Priority_base = 0xc,
    Enable0       = 0x18,
    Enable1       = 0x1C,
    Treshold0     = 0x20,
    Treshold1     = 0x24,
    Pending0      = 0x28,
    Pending1      = 0x2C,
  };

  enum
  {
    Num_irqs_mask       = 0xffff,
    Num_targets_mask    = 0xffff,
    Num_targets_shift   = 16,
    Num_priorities_mask = 0xffff,
    Has_thresholds_mask = 0x10000,

    Priority_mask  = 0xf,
  };

  struct R_config0
  {
    Unsigned32 _raw;
    explicit R_config0(Unsigned32 raw) : _raw(raw) {}

    CXX_BITFIELD_MEMBER( 0, 15, num_irqs, _raw);
    CXX_BITFIELD_MEMBER(16, 31, num_targets, _raw);
  };

  struct R_config1
  {
    Unsigned32 _raw;
    explicit R_config1(Unsigned32 raw) : _raw(raw) {}

    CXX_BITFIELD_MEMBER( 0, 15, num_priorities, _raw);
    CXX_BITFIELD_MEMBER(16, 16, has_thresholds, _raw);
  };

  Register_block<32> _regs;
  unsigned _num_priorities;

  // XXX Use Io_spin_lock in the case that Mem::mp_acquire/release()
  // do not include the I/O space.
  // XXX Use atomic operations instead of lock, they also work on I/O memory.
  // XXX Or just use CPU lock instead (MiG-V has only a single core...)
  Spin_lock<> _lock;
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>

PUBLIC inline
Irq_chip_migv::Irq_chip_migv(void *mmio)
  : Irq_chip_gen(Num_irqs), _regs(mmio)
{
  R_config0 config0(_regs[Config0]);
  R_config1 config1(_regs[Config1]);

  assert(config0.num_irqs() == Num_irqs);
  assert(config0.num_targets() == Num_targets);
  _num_priorities = config1.num_priorities();

  // Disable all interrupts for machine and supervisor mode
  _regs[Enable0] = 0;
  _regs[Enable1] = 0;

  if (config1.has_thresholds())
    {
      // Set priority threshold to minimum value.
      _regs[Treshold0] = 0;
      _regs[Treshold1] = 0;
    }

  // Set default priority for all interrupt sources.
  for (unsigned pin = 1; pin <= nr_pins(); pin++)
    priority(pin, 1);
}

PRIVATE static inline
unsigned
Irq_chip_migv::pin_bit(Mword pin)
{
  assert(pin >= 1);
  assert(pin <= Num_irqs);
  return 1 << (pin - 1);
}

PUBLIC
bool
Irq_chip_migv::handle_pending()
{
  Unsigned32 pending = _regs[Pending1];
  if (!pending)
    return false;

  handle_irq<Irq_chip_migv>(pending, nullptr);
  return true;
}

PRIVATE static inline
unsigned
Irq_chip_migv::priority_offset(Mword pin)
{
  assert(pin >= 1);
  assert(pin <= Num_irqs);
  // Each priority register spans 8 pins
  return Priority_base + ((pin - 1) / 8) * sizeof(Unsigned32);
}

PRIVATE static inline
unsigned
Irq_chip_migv::priority_shift(Mword pin)
{
  assert(pin >= 1);
  assert(pin <= Num_irqs);
  // Each priority spans 4 bits
  return ((pin - 1) % 8) * 4;
}

PUBLIC
Unsigned32
Irq_chip_migv::priority(Mword pin)
{
  return (_regs[priority_offset(pin)] >> priority_shift(pin)) & Priority_mask;
}

PUBLIC
void
Irq_chip_migv::priority(Mword pin, Unsigned32 priority)
{
  assert(priority >= 1);
  assert(priority <= _num_priorities);

  auto guard = lock_guard(_lock);
  unsigned shift = priority_shift(pin);
  _regs.r<Unsigned32>(priority_offset(pin)).modify(
    (priority & Priority_mask) << shift, Priority_mask << shift);
}

PUBLIC
void
Irq_chip_migv::mask(Mword pin) override
{
  auto guard = lock_guard(_lock);
  _regs.r<Unsigned32>(Enable1).clear(pin_bit(pin));
}

PUBLIC
void
Irq_chip_migv::unmask(Mword pin) override
{
  auto guard = lock_guard(_lock);
  _regs.r<Unsigned32>(Enable1).set(pin_bit(pin));
}

PUBLIC
void
Irq_chip_migv::ack(Mword) override
{}

PUBLIC
void
Irq_chip_migv::mask_and_ack(Mword pin) override
{
  assert (cpu_lock.test());

  mask(pin);
  ack(pin);
}

PUBLIC
int
Irq_chip_migv::set_mode(Mword pin, Mode mode) override
{
  auto guard = lock_guard(_lock);

  if (mode.level_triggered())
    _regs.r<Unsigned32>(Trigger_mode).clear(pin_bit(pin));
  else
    _regs.r<Unsigned32>(Trigger_mode).set(pin_bit(pin));

  return 0;
}

PUBLIC
bool
Irq_chip_migv::is_edge_triggered(Mword pin) const override
{
  // "A logic '1' denotes the interrupt to be rising-edge triggered."
  return _regs[Trigger_mode] & pin_bit(pin);
}

PUBLIC
void
Irq_chip_migv::set_cpu(Mword pin, Cpu_number cpu) override
{
  // XXX Maybe also the invalid CPU is possible here?
  assert(cpu == Cpu_number::boot_cpu());
  unmask(pin);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
char const *
Irq_chip_migv::chip_type() const override
{ return "MiG-V"; }
