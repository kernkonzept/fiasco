INTERFACE [ia32 || amd64 || ux]:

#include "l4_types.h"

EXTENSION class Clock_base
{
protected:
  typedef Unsigned64 Counter;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64 || ux]:

#include "cpu.h"

IMPLEMENT inline NEEDS["cpu.h"]
Clock::Counter
Clock::read_counter() const
{
  return Cpu::rdtsc();
}

IMPLEMENT inline NEEDS["cpu.h"]
Cpu_time
Clock::us(Time t)
{
  return Cpu::cpus.cpu(_cpu_id).tsc_to_us(t);
}
