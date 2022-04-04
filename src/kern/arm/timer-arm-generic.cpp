// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer]:

#include "generic_timer.h"

EXTENSION class Timer
{
public:
  typedef Generic_timer::Gtimer Gtimer;

private:
  enum
  {
    CTL_ENABLE   = 1 << 0,
    CTL_IMASK    = 1 << 1,
    CTL_ISTATUS  = 1 << 2,
  };

  static void bsp_init(Cpu_number);
  static Unsigned32 frequency();

  static Mword _interval;
  static Mword _freq0;
};

// --------------------------------------------------------------
IMPLEMENTATION [arm && arm_generic_timer]:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "io.h"

Mword Timer::_interval;
Mword Timer::_freq0;

IMPLEMENT_OVERRIDE
Irq_chip::Mode Timer::irq_mode()
{
  // Some sources describe this IRQ as "level/low" but the GIC code only allows
  // "level/high" or "edge/high". The GIC redistributor doesn't distinguish
  // between "low" and "high" so just use the accepted level-sensitive value.
  return Irq_chip::Mode::F_level_high;
}

IMPLEMENT_DEFAULT
static inline
Unsigned32 Timer::frequency()
{ return Gtimer::frequency(); }

IMPLEMENT
void Timer::init(Cpu_number cpu)
{
  if (!Cpu::cpus.cpu(cpu).has_generic_timer())
    panic("CPU does not support the ARM generic timer");

  if (Proc::Is_hyp)
    Generic_timer::T<Generic_timer::Virtual>::control(0);

  Gtimer::control(0);

  bsp_init(cpu);

  if (cpu == Cpu_number::boot_cpu())
    {
      _freq0 = frequency();
      _interval = (Unsigned64)_freq0 * Config::Scheduler_granularity / 1000000;
      printf("ARM generic timer: freq=%ld interval=%ld cnt=%lld\n", _freq0, _interval, Gtimer::counter());
      assert(_freq0);

      freq_to_scaler_shift(1000000000, _freq0,
                           &_scaler_ts_to_ns, &_shift_ts_to_ns);
      freq_to_scaler_shift(1000000, _freq0,
                           &_scaler_ts_to_us, &_shift_ts_to_us);
    }
  else if (_freq0 != frequency())
    {
      printf("Different frequency on AP CPUs");
      Gtimer::frequency(_freq0);
    }

  Gtimer::setup_timer_access();

  // wait for timer to really start counting
  Unsigned64 v = Gtimer::counter();
  while (Gtimer::counter() == v)
    ;
}

IMPLEMENT_OVERRIDE
void
Timer::enable()
{
  Gtimer::compare(Gtimer::counter() + _interval);
  Gtimer::control(CTL_ENABLE);
}

PUBLIC static inline
void Timer::acknowledge()
{
  Gtimer::compare(Gtimer::compare() + _interval);
}

IMPLEMENT_OVERRIDE inline
Unsigned64
Timer::time_stamp()
{ return Gtimer::counter(); }

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && jdb]:

EXTENSION class Timer
{
private:
  static Mword _using_interval_jdb;
};


// --------------------------------------------------------------
IMPLEMENTATION [arm && arm_generic_timer && jdb]:

Mword Timer::_using_interval_jdb = false;

IMPLEMENT_OVERRIDE
void
Timer::switch_freq_jdb()
{
  if (mp_cas(&_using_interval_jdb, (Mword)false, (Mword)true))
    _interval *= 10;
}

IMPLEMENT_OVERRIDE
void
Timer::switch_freq_system()
{
  if (mp_cas(&_using_interval_jdb, (Mword)true, (Mword)false))
    _interval /= 10;
}
