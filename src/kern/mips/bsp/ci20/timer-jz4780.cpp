INTERFACE:

#include "irq_chip.h"

IMPLEMENTATION:

#include "cpu.h"
#include "config.h"
#include "globals.h"
#include "kip.h"
#include "warn.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "tcu_jz4780.h"

static Static_object<Tcu_jz4780> _tcu;

IMPLEMENT
void
Timer::init(Cpu_number ncpu)
{
  typedef Tcu_jz4780 T;
  if (ncpu != Cpu_number::boot_cpu())
    return;

  init_system_clock();
  _tcu.construct(0xb0002000);
  _tcu->r[T::TSCR]    = 1 << 15;
  _tcu->r[T::OSTCSR]  = 2 << 3;  // prescaler 16, periodic mode
  _tcu->r[T::OSTCNTH] = 0;
  _tcu->r[T::OSTCNTL] = 0;
  _tcu->r[T::OSTCSR].set(4);     // set EXT_EN
  _tcu->r[T::OSTDR]   = 48000000 / 16 / Config::Scheduler_granularity;
}

IMPLEMENT_OVERRIDE
void
Timer::enable()
{
  typedef Tcu_jz4780 T;
  _tcu->r[T::OSTCNTH] = 0;
  _tcu->r[T::OSTCNTL] = 0;
  _tcu->r[T::TFCR]    = 1 << 15;
  _tcu->r[T::TMCR]    = 1 << 15; // clear OSTMCL
  _tcu->r[T::TESR]    = 1 << 15; // set OSTEN
}

PUBLIC static inline NEEDS["irq_chip.h"]
Irq_chip::Mode
Timer::irq_mode()
{ return Irq_chip::Mode::F_level_high; }

PUBLIC static // inline NEEDS[Timer::_set_compare]
void
Timer::acknowledge()
{
  _tcu->r[Tcu_jz4780::TFCR] = 1 << 15;
}

IMPLEMENT inline NEEDS ["kip.h"]
void
Timer::init_system_clock()
{
  // FIXME: may be sync to counter register
  Kip::k()->clock = 0;
}

IMPLEMENT inline NEEDS ["kip.h"]
Unsigned64
Timer::system_clock()
{
  // FIXME: use local counter directly
  return Kip::k()->clock;
}

IMPLEMENT inline NEEDS ["config.h", "kip.h"]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    Kip::k()->clock += Config::Scheduler_granularity;
}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64 wakeup)
{
  (void) wakeup;
  // FIXEME: need this for one-shot mode
}

PUBLIC static inline
unsigned
Timer::irq()
{ return 27; }

PUBLIC static //inline
Unsigned64
Timer::get_current_counter()
{
  Unsigned32 lo = _tcu->r[Tcu_jz4780::OSTCNTL];
  Unsigned32 hi = _tcu->r[Tcu_jz4780::OSTCNTH];

  return (((Unsigned64)hi) << 32) | lo;
}
