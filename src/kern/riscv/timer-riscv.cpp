INTERFACE [riscv]:

#include "cpu.h"
#include "per_cpu_data.h"

EXTENSION class Timer
{
private:
  enum : Unsigned32
  {
    Microsec_per_sec = 1000000,
  };

  // XXX Make the ticks <-> us conversions more efficient.
  static Unsigned32 _freq_scaler;
  static Unsigned32 _us_scaler;

  static Unsigned64 us_to_ticks(Unsigned64 us)
  { return (us * _freq_scaler) / _us_scaler; }

  static Unsigned64 ticks_to_us(Unsigned64 ticks)
  { return (ticks * _us_scaler) / _freq_scaler; }

  // Period of periodic timer.
  static Unsigned64 _timer_period;
  static Per_cpu<Unsigned64> _timer_next_trigger;

  // Tracks whether the timer is enabled. Required for one-shot timer mode, as
  // there the timer interrupt is temporarily disabled until the next wakeup
  // event is programmed. Not used in periodic timer emulation mode because
  // there the timer interrupt enable flag can be used directly to store the
  // timer enable status.
  static Per_cpu<bool> _enabled;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "kip.h"
#include "panic.h"
#include "sbi.h"
#include "warn.h"

Unsigned32 Timer::_freq_scaler;
Unsigned32 Timer::_us_scaler;
Unsigned64 Timer::_timer_period;
DEFINE_PER_CPU Per_cpu<Unsigned64> Timer::_timer_next_trigger;
DEFINE_PER_CPU Per_cpu<bool> Timer::_enabled;

PRIVATE static
Unsigned32
Timer::gcd(Unsigned32 a, Unsigned32 b)
{
  Unsigned32 r;
  while (b != 0)
    {
      r = a % b;
      a = b;
      b = r;
    }
  return a;
}

IMPLEMENT
void
Timer::init(Cpu_number cpu)
{
  // The timer may only be initialized for the current CPU.
  assert(cpu == Proc::cpu());

  if (cpu == Cpu_number::boot_cpu())
    {
      Unsigned32 frequency = Kip::k()->platform_info.arch.timebase_frequency;
      if (frequency == 0)
        panic("Invalid timer frequency!");

      _freq_scaler = frequency;
      _us_scaler = Microsec_per_sec;

      // Squash down scalers to avoid overflow while preserving precision
      Unsigned32 div = gcd(_freq_scaler, _us_scaler);
      _freq_scaler /= div;
      _us_scaler /= div;

      Unsigned64 secs_till_overflow =
        ~0ULL / (static_cast<Unsigned64>(frequency) * _us_scaler);
      // ~135 years till overflow
      if (secs_till_overflow < (1ULL << 32))
        panic("Failed to calibrate timer!");

      if (!Config::Scheduler_one_shot)
        _timer_period = us_to_ticks(Config::Scheduler_granularity);
    }
}

IMPLEMENT_OVERRIDE inline
void
Timer::init_system_clock()
{
  update_system_clock(Cpu_number::boot_cpu());
}

IMPLEMENT inline NEEDS[Timer::update_system_clock]
void
Timer::update_system_clock(Cpu_number cpu)
{
  update_system_clock(cpu, Cpu::rdtime());
}

PRIVATE static inline NEEDS["kip.h"]
void
Timer::update_system_clock(Cpu_number cpu, Unsigned64 time)
{
  // When Fiasco runs in one-shot timer mode, CPUs potentially have a reduced
  // timer interrupt frequency. As a consequence the boot CPU only updates the
  // KIP clock irregularly, which can negatively influence the behavior of the
  // whole system. For example, kernel timeout processing relies on the KIP
  // clock. Therefore, as a workaround, we update the KIP clock from all CPUs
  // when running in one-shot timer mode.
  //
  // Regarding a one-shot timer mode rework: A proper solution would be to
  // directly use Cpu::rdtime() instead of KIP clock. In addition, user mode
  // needs a different time source than the KIP clock, which is reliable also
  // under a one-shot timer.
  if (Config::Scheduler_one_shot || cpu == Cpu_number::boot_cpu())
    Kip::k()->set_clock(ticks_to_us(time));
}

IMPLEMENT inline NEEDS["sbi.h"]
void
Timer::update_timer(Unsigned64 wakeup_us)
{
  if (Config::Scheduler_one_shot)
    {
      Sbi::set_timer(us_to_ticks(wakeup_us));
      if (_enabled.current())
        Cpu::enable_timer_interrupt(true);
    }
}

PRIVATE static inline NEEDS["sbi.h"]
void
Timer::reprogram_periodic_timer(Cpu_number cpu)
{
  // Calculate next trigger time of timer
  Unsigned64 next_trigger = _timer_next_trigger.cpu(cpu) + _timer_period;

  _timer_next_trigger.cpu(cpu) = next_trigger;
  Sbi::set_timer(next_trigger);
}

PUBLIC static
void
Timer::acknowledge()
{}

PUBLIC static inline NEEDS[Timer::reprogram_periodic_timer]
void
Timer::handle_interrupt()
{
  auto cpu = current_cpu();
  if (Config::Scheduler_one_shot)
    {
      // Disable timer interrupts as the STIP flag is not cleared
      // until the next call to sbi_set_timer.
      Cpu::enable_timer_interrupt(false);
    }
  else
    {
      // Emulate periodic timer
      reprogram_periodic_timer(cpu);
    }

  Timer::update_system_clock(cpu, Cpu::rdtime());
}

PUBLIC static
void
Timer::toggle(Cpu_number cpu, bool enable)
{
  if (cpu != current_cpu())
    {
      WARN("Cannot enable/disable timer of remote CPU.\n");
      return;
    }

  if (Config::Scheduler_one_shot)
    _enabled.current() = enable;
  else /* periodic timer */
    if (enable)
      {
        // Kick off the periodic timer
        _timer_next_trigger.cpu(cpu) = Cpu::rdtime();
        reprogram_periodic_timer(cpu);
      }

  Cpu::enable_timer_interrupt(enable);
}
