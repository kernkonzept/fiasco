// --------------------------------------------------------------------------
INTERFACE [arm && sp804]:

#include "timer_sp804.h"

EXTENSION class Timer
{
private:
  static Static_object<Timer_sp804> sp804;
};

// --------------------------------------------------------------------------
INTERFACE [arm && sp804 && pf_realview_vexpress_a15]:

EXTENSION class Timer
{
public:
  static unsigned irq() { return 34; }
};

// --------------------------------------------------------------------------
INTERFACE [arm && arm_generic_timer]:

EXTENSION class Timer
{
public:
  static unsigned irq()
  {
    switch (Gtimer::Type)
      {
      case Generic_timer::Physical: return 29;
      case Generic_timer::Virtual:  return 27;
      case Generic_timer::Hyp:      return 26;
      };
  }
};

// --------------------------------------------------------------------------
INTERFACE [arm && sp804 && !pf_realview_vexpress_a15]:

EXTENSION class Timer
{
public:
  static unsigned irq() { return 36; }
};

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && sp804]:

#include "config.h"
#include "kip.h"
#include "platform.h"

#include <cstdio>

Static_object<Timer_sp804> Timer::sp804;

IMPLEMENT
void Timer::init(Cpu_number)
{
  sp804.construct(Kmem::mmio_remap(Mem_layout::Timer0_phys_base));
  Platform::system_control->modify<Mword>(Platform::System_control::Timer0_enable, 0, 0);

  // all timers off
  sp804->disable();
  //Io::write<Mword>(0, Timer_sp804::Ctrl_1);
  //Io::write<Mword>(0, Timer_sp804::Ctrl_2);
  //Io::write<Mword>(0, Timer_sp804::Ctrl_3);

  sp804->reload_value(Timer_sp804::Interval);
  sp804->counter_value(Timer_sp804::Interval);
  sp804->enable(Timer_sp804::Ctrl_periodic | Timer_sp804::Ctrl_ie);
}

PUBLIC static inline
void
Timer::acknowledge()
{
  sp804->irq_clear();
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
  else
    return Kip::k()->clock;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_generic_timer]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
