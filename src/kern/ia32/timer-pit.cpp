INTERFACE [{ia32,amd64}-pit_timer]:

#include "irq_chip.h"

IMPLEMENTATION[{ia32,amd64}-pit_timer]:

#include "pit.h"

#include <cstdio>

IMPLEMENT
void
Timer::init(Cpu_number)
{
  printf("Using the PIT (i8254) on IRQ %d for scheduling\n", irq());

  // set up timer interrupt (~ 1ms)
  Pit::init();
}

PUBLIC static inline
unsigned Timer::irq() { return 0; }

PUBLIC static inline NEEDS["irq_chip.h"]
Irq_chip::Mode Timer::irq_mode()
{ return Irq_chip::Mode::F_raising_edge; }

PUBLIC static inline
void
Timer::acknowledge()
{}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64)
{
  // does nothing in periodic mode
}
