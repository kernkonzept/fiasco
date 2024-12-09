INTERFACE [riscv]:

#include "cpu.h"
#include "per_cpu_data.h"
#include "scaler_shift.h"

EXTENSION class Timer
{
  friend class Kip_test;

public:
  static Scaler_shift get_scaler_shift_ts_to_ns()
  { return _scaler_shift_ts_to_ns; }

  static Scaler_shift get_scaler_shift_ts_to_us()
  { return _scaler_shift_ts_to_us; }

  static Unsigned64 us_to_ts(Unsigned64 us)
  { return _scaler_shift_us_to_ts.transform(us); }

  static Unsigned64 ts_to_us(Unsigned64 ts)
  { return _scaler_shift_ts_to_us.transform(ts); }

  static Unsigned64 ts_to_ns(Unsigned64 ts)
  { return _scaler_shift_ts_to_ns.transform(ts); }

private:
  enum : Unsigned32
  {
    Microsec_per_sec = 1'000'000,
    Nanosec_per_sec  = 1'000'000'000,
  };

  static Scaler_shift _scaler_shift_us_to_ts;
  static Scaler_shift _scaler_shift_ts_to_us;
  static Scaler_shift _scaler_shift_ts_to_ns;

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

Scaler_shift Timer::_scaler_shift_us_to_ts;
Scaler_shift Timer::_scaler_shift_ts_to_us;
Scaler_shift Timer::_scaler_shift_ts_to_ns;
Unsigned64 Timer::_timer_period;
DEFINE_PER_CPU Per_cpu<Unsigned64> Timer::_timer_next_trigger;
DEFINE_PER_CPU Per_cpu<bool> Timer::_enabled;

IMPLEMENT
void
Timer::init(Cpu_number cpu)
{
  // The timer may only be initialized for the current CPU.
  assert(cpu == Proc::cpu());

  if (cpu == Cpu_number::boot_cpu())
    {
      Unsigned32 freq = Kip::k()->platform_info.arch.timebase_frequency;
      if (freq == 0)
        panic("Invalid timer frequency!");

      _scaler_shift_us_to_ts = Scaler_shift::calc(Microsec_per_sec, freq);
      _scaler_shift_ts_to_us = Scaler_shift::calc(freq, Microsec_per_sec);
      _scaler_shift_ts_to_ns = Scaler_shift::calc(freq, Nanosec_per_sec);

      if constexpr (!Config::Scheduler_one_shot)
        _timer_period = us_to_ts(Config::Scheduler_granularity);
    }

  // Allow user-mode  read access to counter registers.
  csr_write(scounteren,
            Cpu::Scounteren_cy | Cpu::Scounteren_tm | Cpu::Scounteren_ir);
  if (TAG_ENABLED(sync_clock) && !(csr_read(scounteren) & Cpu::Scounteren_tm))
    panic("User-mode access of time CSR not supported.");
}

PRIVATE static inline NEEDS["sbi.h"]
void
Timer::set_timer(Unsigned64 time_value)
{
  if (Cpu::has_isa_ext(Cpu::Isa_ext_sstc))
    csr_write64(stimecmp, time_value);
  else
    Sbi::set_timer(time_value);
}

IMPLEMENT inline NEEDS[Timer::set_timer]
void
Timer::update_timer(Unsigned64 wakeup_us)
{
  if constexpr (Config::Scheduler_one_shot)
    {
      if (wakeup_us == Infinite_timeout)
        set_timer(0xffff'ffff'ffff'ffffULL);
      else
        set_timer(us_to_ts(wakeup_us));
      if (_enabled.current())
        Cpu::enable_timer_interrupt(true);
    }
}

PRIVATE static inline NEEDS[Timer::set_timer]
void
Timer::reprogram_periodic_timer(Cpu_number cpu)
{
  // Calculate next trigger time of timer
  Unsigned64 next_trigger = _timer_next_trigger.cpu(cpu) + _timer_period;

  _timer_next_trigger.cpu(cpu) = next_trigger;
  set_timer(next_trigger);
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
  if constexpr (Config::Scheduler_one_shot)
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

  Timer::update_system_clock(cpu);
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

  if constexpr (Config::Scheduler_one_shot)
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

// ------------------------------------------------------------------------
IMPLEMENTATION [riscv && sync_clock]:

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock()
{
  Cpu_time time = ts_to_us(Cpu::rdtime());
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on boot CPU\n", time);
}

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock_ap(Cpu_number cpu)
{
  Cpu_time time = ts_to_us(Cpu::rdtime());
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on CPU%u\n",
         time, cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE inline NEEDS["kip.h"]
Unsigned64
Timer::system_clock()
{
  return ts_to_us(Cpu::rdtime());
}

IMPLEMENT inline
Unsigned64
Timer::aux_clock_unstopped()
{
  return ts_to_us(Cpu::rdtime());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [riscv && !sync_clock]:

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
  if (cpu == Cpu_number::boot_cpu())
    Kip::k()->set_clock(ts_to_us(time));
}
