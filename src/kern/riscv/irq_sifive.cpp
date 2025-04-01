INTERFACE:

#include <cassert>

#include "cpu.h"
#include "irq_chip_generic.h"
#include "kip.h"
#include "mmio_register_block.h"
#include "per_cpu_data.h"


/*
  Driver for the RISC-V PLIC as described in the privileged architecture
  specification.

  The actual layout is specified in chapter 8 of the SiFive U5 Coreplex
  Series Manual: https://static.dev.sifive.com/U54-MC-RVCoreIP.pdf
*/
class Irq_chip_sifive : public Irq_chip_gen
{
private:
  enum : Address
  {
    Priority_base     = 0x0,
    Pending_base      = 0x1000,

    Enable_base       = 0x2000,
    Enable_per_hart   = 0x80,

    Context_base      = 0x200000,
    Context_threshold = 0x0,
    Context_claim     = 0x4,
    Context_per_hart  = 0x1000,
  };

  void *_mmio;
  Register_block<32> _priority;

  // XXX Use Io_spin_lock in the case that Mem::mp_acquire/release()
  // do not include the I/O space.
  // XXX Use atomic operations instead of lock, they also work on I/O memory.
  Spin_lock<> _lock;

  class Plic_target
  {
  public:
    void init(void *mmio, Mword context_nr, unsigned nr_pins)
    {
      _enable.set_mmio_base(
        offset_cast<void *>(mmio, Enable_base + context_nr * Enable_per_hart));
      _context.set_mmio_base(
        offset_cast<void *>(mmio, Context_base + context_nr * Context_per_hart));

      // Set priority threshold to minimum value.
      threshold(0);

      // Disable all interrupt sources by default.
      for (unsigned pin = 1; pin < nr_pins; pin++)
        mask(pin);
    }

    void mask(Mword pin)
    { _enable.r<Unsigned32>(pin_offset(pin)).clear(pin_bit(pin)); }

    void unmask(Mword pin)
    { _enable.r<Unsigned32>(pin_offset(pin)).set(pin_bit(pin)); }

    Unsigned32 claim()
    { return _context[Context_claim]; }

    void complete(Mword pin)
    {
      // Interrupt has to be enabled, otherwise complete has no effect.
      assert(_enable[pin_offset(pin)] & pin_bit(pin));

      _context[Context_claim] = static_cast<Unsigned32>(pin);
    }

    Unsigned32 threshold() const
    { return _context[Context_threshold]; }

    void threshold(Unsigned32 threshold)
    { _context[Context_threshold] = threshold; }

  private:
    static unsigned pin_offset(Mword pin)
    { return (pin / 32) * sizeof(Unsigned32); }

    static unsigned pin_bit(Mword pin)
    { return 1 << (pin % 32); }

    Register_block<32> _enable;
    Register_block<32> _context;
  };

  static Per_cpu<Plic_target> targets;
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_PER_CPU Per_cpu<Irq_chip_sifive::Plic_target> Irq_chip_sifive::targets;

PUBLIC inline
Irq_chip_sifive::Irq_chip_sifive(void *mmio)
  : Irq_chip_gen(Kip::k()->platform_info.arch.plic_nr_pins),
    _mmio(mmio),
    _priority(offset_cast<void *>(mmio, Priority_base))
{
  init_cpu(Cpu_number::boot_cpu());

  // Set default priority for all interrupt sources.
  for (unsigned pin = 1; pin < nr_pins(); pin++)
    priority(pin, 1);
}

PUBLIC
void
Irq_chip_sifive::init_cpu(Cpu_number cpu)
{
  int hart_idx = Kip::k()->hart_idx(
    cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id()));
  assert(hart_idx >= 0);

  auto hart_context =
    Kip::k()->platform_info.arch.plic_hart_irq_targets[hart_idx];
  targets.cpu(cpu).init(_mmio, hart_context, nr_pins());
}

PRIVATE inline
Irq_chip_sifive::Plic_target &
Irq_chip_sifive::current_target()
{
  // We can't use targets.current() here, because early in the startup process
  // no context is setup, so the underlying current_cpu() function does
  // not work.
  return targets.cpu(Proc::cpu());
}

PUBLIC
bool
Irq_chip_sifive::handle_pending()
{
  Unsigned32 pending = current_target().claim();
  if (!pending)
    return false;

  handle_irq<Irq_chip_sifive>(pending, nullptr);
  return true;
}

PUBLIC
Unsigned32
Irq_chip_sifive::priority(Mword pin) const
{
  return _priority[pin * sizeof(Unsigned32)];
}

PUBLIC
void
Irq_chip_sifive::priority(Mword pin, Unsigned32 priority)
{
  _priority[pin * sizeof(Unsigned32)] = priority;
}

PUBLIC
void
Irq_chip_sifive::mask(Mword pin) override
{
  auto guard = lock_guard(_lock);
  current_target().mask(pin);
}

PUBLIC
void
Irq_chip_sifive::unmask(Mword pin) override
{
  auto guard = lock_guard(_lock);
  current_target().unmask(pin);
}

PUBLIC
void
Irq_chip_sifive::ack(Mword pin) override
{
  current_target().complete(pin);
}

PUBLIC
void
Irq_chip_sifive::mask_and_ack(Mword pin) override
{
  assert (cpu_lock.test());

  // We can't ack a masked interrupts, therefore we have to ack before masking.
  // "If the completion ID does not match an interrupt source that is currently
  //  enabled for the target, the completion is silently ignored."
  ack(pin);
  mask(pin);
}

PUBLIC
int
Irq_chip_sifive::set_mode(Mword, Mode) override
{
  return 0;
}

PUBLIC
bool
Irq_chip_sifive::is_edge_triggered(Mword) const override
{ return false; }

PUBLIC
void
Irq_chip_sifive::set_cpu(Mword pin, Cpu_number cpu) override
{
  auto guard = lock_guard(_lock);

  for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
    {
      if (!Per_cpu_data::valid(n))
        continue;

      if (n == cpu)
        targets.cpu(n).unmask(pin);
      else
        targets.cpu(n).mask(pin);
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
char const *
Irq_chip_sifive::chip_type() const override
{ return "SiFive"; }
