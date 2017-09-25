// --------------------------------------------------------------------------
INTERFACE [arm && integrator]:

#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static unsigned irq() { return 6; }

private:
  enum {
    TIMER0_BASE = 0x000,
    TIMER1_BASE = 0x100,
    TIMER2_BASE = 0x200,

    TIMER_LOAD   = 0x00,
    TIMER_VALUE  = 0x04,
    TIMER_CTRL   = 0x08,
    TIMER_INTCLR = 0x0c,

    TIMER_CTRL_IE       = 1 << 5,
    TIMER_CTRL_PERIODIC = 1 << 6,
    TIMER_CTRL_ENABLE   = 1 << 7,
  };

  static Static_object<Timer> _timer;
};

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && integrator]:

#include "config.h"
#include "kip.h"
#include "kmem.h"
#include "mem_layout.h"

Static_object<Timer> Timer::_timer;

PUBLIC
Timer::Timer(Address base) : Mmio_register_block(base)
{
  /* Switch all timers off */
  write(0, TIMER0_BASE + TIMER_CTRL);
  write(0, TIMER1_BASE + TIMER_CTRL);
  write(0, TIMER2_BASE + TIMER_CTRL);

  unsigned timer_ctrl = TIMER_CTRL_ENABLE | TIMER_CTRL_PERIODIC;
  unsigned timer_reload = 1000000 / Config::Scheduler_granularity;

  write(timer_reload, TIMER1_BASE + TIMER_LOAD);
  write(timer_reload, TIMER1_BASE + TIMER_VALUE);
  write(timer_ctrl | TIMER_CTRL_IE, TIMER1_BASE + TIMER_CTRL);
}

IMPLEMENT
void Timer::init(Cpu_number)
{ _timer.construct(Kmem::mmio_remap(Mem_layout::Timer_phys_base)); }

PUBLIC static inline
void
Timer::acknowledge()
{
  _timer->write(1, TIMER1_BASE + TIMER_INTCLR);
}

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 wakeup)
{
  (void)wakeup;
}

IMPLEMENT inline NEEDS["kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  else
    return Kip::k()->clock;
}

