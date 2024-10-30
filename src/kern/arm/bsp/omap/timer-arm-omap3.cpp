// --------------------------------------------------------------------------
INTERFACE [arm && pf_omap3_35x]:

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

INTERFACE [arm && pf_omap3_am33xx]: //----------------------------------------

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

IMPLEMENTATION [pf_omap3]: // ------------------------------------------------

Static_object<Timer_omap_1mstimer> Timer::_timer;

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && pf_omap3_35x]:

#include "kmem_mmio.h"

IMPLEMENT
void
Timer::init(Cpu_number)
{
  // select 32768 Hz input to GPTimer1 (timer1 only!)
  Mmio_register_block wkup(Kmem_mmio::map(CM_CLKSEL_WKUP, 0x10));
  wkup.modify<Mword>(0, 1, 0);
  _timer.construct(true);
}

PUBLIC static inline
void
Timer::acknowledge()
{
  _timer->acknowledge();
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && pf_omap3_am33xx]:

#include "kmem_mmio.h"
#include "mem_layout.h"

Static_object<Timer_omap_gentimer> Timer::_gentimer;

IMPLEMENT
void
Timer::init(Cpu_number)
{
  Mmio_register_block wkup(Kmem_mmio::map(Mem_layout::Cm_wkup_phys_base,
                                          0x100));
  Mmio_register_block clksel(Kmem_mmio::map(Mem_layout::Cm_dpll_phys_base,
                                            0x100));
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
