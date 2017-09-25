// --------------------------------------------------------------------------
INTERFACE [arm && omap3_35x]:

#include "timer_omap_1mstimer.h"
#include "mem_layout.h"

EXTENSION class Timer
{
public:
  static unsigned irq() { return 37; }

private:
  enum {
    CM_CLKSEL_WKUP = Mem_layout::Wkup_cm_phys_base + 0x40,
  };

  static Static_object<Timer_omap_1mstimer> _timer;
};

INTERFACE [arm && omap3_am33xx]: //----------------------------------------

#include "timer_omap_1mstimer.h"
#include "timer_omap_gentimer.h"

EXTENSION class Timer
{
public:
  static unsigned irq()
  {
    switch (type())
      {
      case Timer0: default: return 66;
      case Timer1_1ms: return 67;
      }
  }

private:
  enum Timer_type { Timer0, Timer1_1ms };
  static Timer_type type() { return Timer1_1ms; }
  enum {
    CM_WKUP_CLKSTCTRL         = 0x00,
    CM_WKUP_TIMER0_CLKCTRL    = 0x10,
    CM_WKUP_TIMER1_CLKCTRL    = 0xc4,
    CLKSEL_TIMER1MS_CLK       = 0x28,

    CLKSEL_TIMER1MS_CLK_OSC   = 0,
    CLKSEL_TIMER1MS_CLK_32KHZ = 1,
    CLKSEL_TIMER1MS_CLK_VALUE = CLKSEL_TIMER1MS_CLK_OSC,
  };
  static Static_object<Timer_omap_1mstimer> _timer;
  static Static_object<Timer_omap_gentimer> _gentimer;
};

IMPLEMENTATION [omap3]: // ------------------------------------------------

Static_object<Timer_omap_1mstimer> Timer::_timer;

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && omap3_35x]:

#include "kmem.h"

IMPLEMENT
void
Timer::init(Cpu_number)
{
  // select 32768 Hz input to GPTimer1 (timer1 only!)
  Mmio_register_block(Kmem::mmio_remap(CM_CLKSEL_WKUP)).modify(0, 1, 0);
  _timer.construct(true);
}

PUBLIC static inline
void
Timer::acknowledge()
{
  _timer->acknowledge();
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && omap3_am33xx]:

#include "kmem.h"
#include "mem_layout.h"

Static_object<Timer_omap_gentimer> Timer::_gentimer;

IMPLEMENT
void
Timer::init(Cpu_number)
{
  Mmio_register_block wkup(Kmem::mmio_remap(Mem_layout::Cm_wkup_phys_base));
  Mmio_register_block clksel(Kmem::mmio_remap(Mem_layout::Cm_dpll_phys_base));
  switch (type())
    {
    case Timer1_1ms:
      // enable DMTIMER1_1MS
      wkup.write<Mword>(2, CM_WKUP_TIMER1_CLKCTRL);
      wkup.read<Mword>(CM_WKUP_TIMER1_CLKCTRL);
      clksel.write<Mword>(CLKSEL_TIMER1MS_CLK_VALUE, CLKSEL_TIMER1MS_CLK);
      for (int i = 0; i < 1000000; ++i) // instead, poll proper reg
        asm volatile("" : : : "memory");

      _timer.construct(CLKSEL_TIMER1MS_CLK_VALUE == CLKSEL_TIMER1MS_CLK_32KHZ);
      break;
    case Timer0:
      wkup.write<Mword>(2, CM_WKUP_TIMER0_CLKCTRL);
      _gentimer.construct();
      break;
    }
}

PUBLIC static inline
void Timer::acknowledge()
{
  if (type() == Timer1_1ms)
    _timer->acknowledge();
  else
    _gentimer->acknowledge();
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && omap3]:

#include "config.h"
#include "kip.h"

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
  else
    return Kip::k()->clock;
}
