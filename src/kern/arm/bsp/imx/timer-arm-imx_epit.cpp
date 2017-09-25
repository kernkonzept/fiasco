// --------------------------------------------------------------------------
INTERFACE [arm && imx_epit]:

#include "timer_imx_epit.h"

EXTENSION class Timer
{
private:
  static Static_object<Timer_imx_epit> _timer;
};


INTERFACE [arm && imx35]: // ----------------------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 28; }
};


INTERFACE [arm && (imx51 || imx53)]: // -----------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 40; }
};

INTERFACE [arm && imx6]: // -----------------------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 88; }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && imx_epit]:

#include "kip.h"
#include "mem_layout.h"

Static_object<Timer_imx_epit> Timer::_timer;

IMPLEMENT
void Timer::init(Cpu_number)
{
  _timer.construct(Mem_layout::Timer_phys_base);
}
PUBLIC static inline
void
Timer::acknowledge()
{
  _timer->acknowledge();
}

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{}

IMPLEMENT inline NEEDS["config.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  else
    return Kip::k()->clock;
}
