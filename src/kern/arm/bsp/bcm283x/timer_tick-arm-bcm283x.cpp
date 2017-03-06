IMPLEMENTATION [arm && (pf_bcm283x_rpi2 || pf_bcm283x_rpi3)]:

#include "timer.h"
#include "arm_control.h"

IMPLEMENT
void
Timer_tick::setup(Cpu_number)
{}

IMPLEMENT
void
Timer_tick::enable(Cpu_number)
{
  Arm_control::o()->timer_unmask(Timer::irq());
  Timer::enable();
}

IMPLEMENT
void
Timer_tick::disable(Cpu_number)
{
  Arm_control::o()->timer_mask(Timer::irq());
}

PUBLIC static inline
void
Timer_tick::ack()
{
  Timer::acknowledge();
}

