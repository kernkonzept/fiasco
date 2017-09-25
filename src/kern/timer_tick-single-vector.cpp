INTERFACE:

#include "types.h"

EXTENSION class Timer_tick
{
public:
  static Static_object<Timer_tick> _glbl_timer;
};

IMPLEMENTATION:

#include "timer.h"

Static_object<Timer_tick> Timer_tick::_glbl_timer;

IMPLEMENT void
Timer_tick::setup(Cpu_number cpu)
{
  // all CPUs use the same timer IRQ, so initialize just on CPU 0
  if (cpu == Cpu_number::boot_cpu())
    _glbl_timer.construct(Any_cpu);

  if (!allocate_irq(_glbl_timer, Timer::irq()))
    panic("Could not allocate scheduling IRQ %d\n", Timer::irq());

  _glbl_timer->chip()->set_mode(_glbl_timer->pin(), Timer::irq_mode());
}

IMPLEMENT
void
Timer_tick::enable(Cpu_number)
{
  _glbl_timer->chip()->unmask(_glbl_timer->pin());
  Timer::enable();
}

IMPLEMENT
void
Timer_tick::disable(Cpu_number)
{
  _glbl_timer->chip()->mask(_glbl_timer->pin());
}

PUBLIC inline NEEDS["timer.h"]
void
Timer_tick::ack()
{
  Timer::acknowledge();
  Irq_base::ack();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

IMPLEMENT
Timer_tick *
Timer_tick::boot_cpu_timer_tick()
{ return _glbl_timer; }
