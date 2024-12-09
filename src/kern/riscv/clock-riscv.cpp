INTERFACE [riscv]:

#include "l4_types.h"

EXTENSION class Clock_base
{
protected:
  typedef Unsigned64 Counter;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "cpu.h"
#include "timer.h"

/**
 * RISC-V-specific implementation using the time CSR.
 */
IMPLEMENT inline NEEDS["cpu.h"]
Clock::Counter
Clock::read_counter() const
{
  return Cpu::rdtime();
}

IMPLEMENT inline NEEDS["timer.h"]
Cpu_time
Clock::us(Time t)
{
  return Timer::ts_to_us(t);
}
