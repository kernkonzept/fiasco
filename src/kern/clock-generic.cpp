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

/**
 * Simple clock implementation reading the KIP clock value.
 *
 * \note This implementation must not be used in one-shot mode.
 */
IMPLEMENT inline NEEDS["kip.h"]
Clock::Counter
Clock::read_counter() const
{
  return Kip::k()->clock();
}

IMPLEMENT inline
Cpu_time
Clock::us(Time t)
{
  return t;
}
