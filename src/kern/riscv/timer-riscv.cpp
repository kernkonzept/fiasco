INTERFACE [riscv]:

#include "cpu.h"

EXTENSION class Timer
{
private:
  enum : Unsigned32
  {
    Microsec_per_sec = 1000000,
  };

  // XXX Make the ticks <-> us conversions more efficient.
  static Unsigned32 freq_scaler;
  static Unsigned32 us_scaler;

  static Unsigned64 us_to_ticks(Unsigned64 us)
  { return (us * freq_scaler) / us_scaler; }

  static Unsigned64 ticks_to_us(Unsigned64 ticks)
  { return (ticks * us_scaler) / freq_scaler; }

  static Unsigned64 timer_period;
  static Per_cpu<Unsigned64> timer_next_trigger;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "kip.h"
#include "panic.h"
#include "sbi.h"

Unsigned32 Timer::freq_scaler;
Unsigned32 Timer::us_scaler;
Unsigned64 Timer::timer_period;
DEFINE_PER_CPU Per_cpu<Unsigned64> Timer::timer_next_trigger;

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

      freq_scaler = frequency;
      us_scaler = Microsec_per_sec;

      // Squash down scalers to avoid overflow while preserving precision
      Unsigned32 div = gcd(freq_scaler, us_scaler);
      freq_scaler /= div;
      us_scaler /= div;

      Unsigned64 secs_till_overflow =
        ~0ULL / (static_cast<Unsigned64>(frequency) * us_scaler);
      // ~135 years till overflow
      if (secs_till_overflow < (1ULL << 32))
        panic("Failed to calibrate timer!");

      if (!Config::Scheduler_one_shot)
        timer_period = us_to_ticks(Config::Scheduler_granularity);
    }

  if (!Config::Scheduler_one_shot)
    {
      // Kick off the "periodic" timer
      auto time = Cpu::rdtime();
      timer_next_trigger.cpu(cpu) = time;
      reprogram_periodic_timer(cpu, time);
      Cpu::enable_timer_interrupt(true);
    }
}

IMPLEMENT inline
void
Timer::init_system_clock()
{
  update_system_clock(Cpu_number::boot_cpu());
}

IMPLEMENT inline NEEDS["kip.h"]
Unsigned64
Timer::system_clock()
{
  return Kip::k()->clock();
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
      Cpu::enable_timer_interrupt(true);
    }
}

PRIVATE static inline NEEDS["sbi.h"]
void
Timer::reprogram_periodic_timer(Cpu_number cpu, Unsigned64 cur_time)
{
  // Calculate next trigger time of timer
  timer_next_trigger.cpu(cpu) += timer_period;

  Unsigned64 next_trigger = timer_next_trigger.cpu(cpu);
  // Make sure that we don't program the one-shot timer to a value in the past
  // (e.g. the current timer interrupt came extremely late).
  if (EXPECT_FALSE(next_trigger <= cur_time))
    // If the next trigger time is in the past, program the timer to a value
    // in the near future instead.
    next_trigger = cur_time + timer_period / 8;

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
  auto time = Cpu::rdtime();
  if (Config::Scheduler_one_shot)
    {
      // Disable timer interrupts as the STIP flag is not cleared
      // until the next call to sbi_set_timer.
      Cpu::enable_timer_interrupt(false);
    }
  else
    {
      // Emulate periodic timer
      reprogram_periodic_timer(cpu, time);
    }

  Timer::update_system_clock(cpu, time);
}
