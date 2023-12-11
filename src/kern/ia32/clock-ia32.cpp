INTERFACE [ia32 || amd64 || ux]:

#include "l4_types.h"

EXTENSION class Clock_base
{
protected:
  typedef Unsigned64 Counter;
};

EXTENSION class Clock
{
private:
  Cpu_number _cpu_id;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64 || ux]:

#include "cpu.h"

IMPLEMENT_OVERRIDE inline
Clock::Clock(Cpu_number cpu)
  : _cpu_id(cpu)
{}

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
