// --------------------------------------------------------------------------
INTERFACE [arm && imx_epit]:

#include "timer_imx_epit.h"

EXTENSION class Timer
{
private:
  static Static_object<Timer_imx_epit> _timer;
};


INTERFACE [arm && pf_imx_35]: // ----------------------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 28; }
};


INTERFACE [arm && (pf_imx_51 || pf_imx_53)]: // -----------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 40; }
};

INTERFACE [arm && pf_imx_6]: // -----------------------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 88; }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && imx_epit]:

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
