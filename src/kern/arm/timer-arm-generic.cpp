// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer]:

#include "generic_timer.h"

EXTENSION class Timer
{
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

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && hyp]:

EXTENSION class Timer { public: typedef Generic_timer::T<Generic_timer::Hyp> Gtimer; };

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && arm_em_tz]:

EXTENSION class Timer { public: typedef Generic_timer::T<Generic_timer::Physical> Gtimer; };

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer && (!hyp && (arm_em_ns || arm_em_std))]:

EXTENSION class Timer { public: typedef Generic_timer::T<Generic_timer::Virtual> Gtimer; };

// --------------------------------------------------------------
IMPLEMENTATION [arm && arm_generic_timer]:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "io.h"
#include "kip.h"

Mword Timer::_interval;
Mword Timer::_freq0;

IMPLEMENT_DEFAULT
static inline
Unsigned32 Timer::frequency()
{
  Unsigned32 v;
  asm volatile ("mrc p15, 0, %0, c14, c0, 0": "=r" (v));
  return v;
}

PUBLIC static void Timer::configure(const Cpu_number &)
{ /* Remove me -- need reinit function */ }

IMPLEMENT
void Timer::init(Cpu_number cpu)
{
  if (!Cpu::cpus.cpu(cpu).has_generic_timer())
    panic("CPU does not support the ARM generic timer");

  bsp_init(cpu);

  if (cpu == Cpu_number::boot_cpu())
    {
      _freq0 = frequency();
      _interval = (Unsigned64)_freq0 * Config::Scheduler_granularity / 1000000;
      printf("ARM generic timer: freq=%ld interval=%ld cnt=%lld\n", _freq0, _interval, Gtimer::counter());
      assert(_freq0);
    }
  else if (_freq0 != frequency())
    {
      printf("Different frequency on AP CPUs");
      asm volatile ("mcr p15, 0, %0, c14, c0, 0": :"r" (_freq0));
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

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 wakeup)
{
  (void)wakeup;
}

IMPLEMENT inline NEEDS["config.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  return Kip::k()->clock;
}
