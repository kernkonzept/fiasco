INTERFACE [arm && arm_generic_timer]:

#include "l4_types.h"

EXTENSION class Clock_base
{
protected:
  typedef Unsigned64 Counter;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_generic_timer]:

#include "generic_timer.h"
#include "timer.h"

/**
 * ARM-specific implementation using the generic timer.
 */
IMPLEMENT inline NEEDS["generic_timer.h"]
Clock::Counter
Clock::read_counter() const
{
  return Generic_timer::Gtimer::counter();
}

IMPLEMENT inline NEEDS["timer.h"]
Cpu_time
Clock::us(Time t)
{
  return Timer::ts_to_us(t);
}
