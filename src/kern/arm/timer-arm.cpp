INTERFACE [arm]:

#include "irq_chip.h"
#include "global_data.h"
#include "scaler_shift.h"

EXTENSION class Timer
{
  friend class Kip_test;

public:
  static Irq_chip::Mode irq_mode();
  static Scaler_shift get_scaler_shift_ts_to_ns() { return _scaler_shift_ts_to_ns; }
  static Scaler_shift get_scaler_shift_ts_to_us() { return _scaler_shift_ts_to_us; }

private:
  static inline void update_one_shot(Unsigned64 wakeup);
  static Unsigned64 time_stamp();
  static Global_data<Scaler_shift> _scaler_shift_ts_to_ns;
  static Global_data<Scaler_shift> _scaler_shift_ts_to_us;
  static Global_data<Scaler_shift> _scaler_shift_us_to_ts;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && vcache]:

#include "mem_unit.h"

PRIVATE static inline NEEDS["mem_unit.h"]
void
Timer::kipclock_cache()
{
  Mem_unit::clean_dcache(static_cast<void *>(const_cast<Cpu_time *>(&Kip::k()->_clock)));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !vcache]:

PRIVATE static inline
void
Timer::kipclock_cache()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "config.h"
#include "context_base.h"
#include "kip.h"
#include "watchdog.h"
#include "warn.h"

DEFINE_GLOBAL Global_data<Scaler_shift> Timer::_scaler_shift_ts_to_ns;
DEFINE_GLOBAL Global_data<Scaler_shift> Timer::_scaler_shift_ts_to_us;
DEFINE_GLOBAL Global_data<Scaler_shift> Timer::_scaler_shift_us_to_ts;

IMPLEMENT_DEFAULT
Unsigned64
Timer::time_stamp()
{ return 0; }

IMPLEMENT_DEFAULT
Irq_chip::Mode Timer::irq_mode()
{ return Irq_chip::Mode::F_raising_edge; }

IMPLEMENT_DEFAULT inline
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{}

IMPLEMENT inline NEEDS[Timer::update_one_shot, "config.h"]
void
Timer::update_timer(Unsigned64 wakeup)
{
  if constexpr (Config::Scheduler_one_shot)
    update_one_shot(wakeup);
}

PUBLIC static inline
Unsigned64
Timer::ts_to_ns(Unsigned64 ts)
{ return _scaler_shift_ts_to_ns->transform(ts); }

PUBLIC static inline
Unsigned64
Timer::ts_to_us(Unsigned64 ts)
{ return _scaler_shift_ts_to_us->transform(ts); }

PUBLIC static inline
Unsigned64
Timer::us_to_ts(Unsigned64 ts)
{ return _scaler_shift_us_to_ts->transform(ts); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !sync_clock]:

IMPLEMENT inline NEEDS["config.h", "kip.h", "watchdog.h", Timer::kipclock_cache]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      Kip::k()->add_to_clock(Config::Scheduler_granularity);
      kipclock_cache();
      Watchdog::touch();
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && sync_clock]:

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock()
{
  Cpu_time time = ts_to_us(time_stamp());
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on boot CPU\n", time);
}

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock_ap(Cpu_number cpu)
{
  Cpu_time time = ts_to_us(time_stamp());
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on CPU%u\n",
         time, cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE inline
Unsigned64
Timer::system_clock()
{
  return ts_to_us(time_stamp());
}

IMPLEMENT inline
Unsigned64
Timer::aux_clock_unstopped()
{
  return ts_to_us(time_stamp());
}
