INTERFACE:

#include "l4_types.h"

EXTENSION class Clock_base
{
protected:
  typedef Cpu_time Counter;
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "kip.h"

IMPLEMENT inline NEEDS["kip.h"]
Clock::Counter
Clock::read_counter() const
{
  return Kip::k()->clock;
}

IMPLEMENT inline
Cpu_time
Clock::us(Time t)
{
  return t;
}
