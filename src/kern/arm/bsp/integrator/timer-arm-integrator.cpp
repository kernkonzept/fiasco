// --------------------------------------------------------------------------
INTERFACE [arm && pf_integrator]:

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
IMPLEMENTATION [arm && pf_integrator]:

#include "config.h"
#include "kmem_mmio.h"
#include "mem_layout.h"

Static_object<Timer> Timer::_timer;

PUBLIC
Timer::Timer(void *base) : Mmio_register_block(base)
{
  /* Switch all timers off */
  write<Unsigned32>(0U, TIMER0_BASE + TIMER_CTRL);
  write<Unsigned32>(0U, TIMER1_BASE + TIMER_CTRL);
  write<Unsigned32>(0U, TIMER2_BASE + TIMER_CTRL);

  unsigned timer_ctrl = TIMER_CTRL_ENABLE | TIMER_CTRL_PERIODIC;
  unsigned timer_reload = 1000000 / Config::Scheduler_granularity;

  write<Unsigned32>(timer_reload, TIMER1_BASE + TIMER_LOAD);
  write<Unsigned32>(timer_reload, TIMER1_BASE + TIMER_VALUE);
  write<Unsigned32>(timer_ctrl | TIMER_CTRL_IE, TIMER1_BASE + TIMER_CTRL);
}

IMPLEMENT
void Timer::init(Cpu_number)
{ _timer.construct(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x300)); }

PUBLIC static inline
void
Timer::acknowledge()
{
  _timer->write<Unsigned32>(1U, TIMER1_BASE + TIMER_INTCLR);
}
