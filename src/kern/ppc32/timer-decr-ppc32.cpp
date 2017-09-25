/**
 * PowerPC timer using internal decrementer
 */
INTERFACE [ppc32]:

#include "irq_chip.h"

IMPLEMENTATION [ppc32]:

#include "cpu.h"
#include "config.h"
#include "globals.h"
#include "kip.h"
#include "decrementer.h"
#include "warn.h"

#include <cstdio>

IMPLEMENT inline NEEDS ["decrementer.h", "kip.h", "config.h", <cstdio>]
void
Timer::init(Cpu_number)
{
  printf("Using PowerPC decrementer for scheduling\n");

  //1000 Hz
  Decrementer::d()->init(Kip::k()->frequency_bus /
                         (4*Config::Scheduler_granularity));

}

PUBLIC static inline
unsigned
Timer::irq()
{ return 0; }

PUBLIC static inline NEEDS["irq_chip.h"]
Irq_chip::Mode
Timer::irq_mode()
{ return Irq_chip::Mode::F_raising_edge; }

PUBLIC static inline
void
Timer::acknowledge()
{ }

//IMPLEMENT inline NEEDS ["decrementer.h"]
//void
//Timer::enable()
//{
//  Decrementer::d()->enable();
//}

//IMPLEMENT inline NEEDS ["decrementer.h"]
//void
//Timer::disable()
//{
//  Decrementer::d()->disable();
//}

IMPLEMENT inline NEEDS ["kip.h"]
void
Timer::init_system_clock()
{
  Kip::k()->clock = 0;
}

IMPLEMENT inline NEEDS ["globals.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  return Kip::k()->clock;
}

IMPLEMENT inline NEEDS ["decrementer.h", "config.h", "kip.h"]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      Decrementer::d()->set();
      Kip::k()->clock += Config::Scheduler_granularity;
    }
}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64)
{
}
