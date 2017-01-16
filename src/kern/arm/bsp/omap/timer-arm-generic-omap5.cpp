IMPLEMENTATION [arm_generic_timer && pf_omap5]:

#include "mem_layout.h"

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 30; // we use this mode in TZ secure mode (so sec IRQ)
    case Generic_timer::Virtual:  return 27;
    case Generic_timer::Hyp:      return 26;
    };
}

IMPLEMENT_OVERRIDE
static inline
Unsigned32 Timer::frequency()
{
  // frequency register is telling us the frequency is 0
  return 6144000;
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
