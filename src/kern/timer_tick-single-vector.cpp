INTERFACE:

#include "types.h"
#include "global_data.h"

EXTENSION class Timer_tick
{
public:
  static Global_data<Static_object<Timer_tick>> _glbl_timer;
};

IMPLEMENTATION:

#include "timer.h"
#include "vkey.h"

DEFINE_GLOBAL Global_data<Static_object<Timer_tick>> Timer_tick::_glbl_timer;

IMPLEMENT void
Timer_tick::setup(Cpu_number cpu)
{
  // all CPUs use the same timer IRQ, so initialize just on CPU 0
  if (cpu == Cpu_number::boot_cpu())
    {
      _glbl_timer.construct(Any_cpu);
      enable_vkey(cpu);
    }

  if (!attach_irq(_glbl_timer, Timer::irq()))
    panic("Could not attach scheduling IRQ %d\n", Timer::irq());

  _glbl_timer->chip()->set_mode_percpu(cpu, _glbl_timer->pin(),
                                       Timer::irq_mode());
}

IMPLEMENT
void
Timer_tick::enable(Cpu_number cpu)
{
  _glbl_timer->chip()->unmask_percpu(cpu, _glbl_timer->pin());
  Timer::enable();
}

IMPLEMENT
void
Timer_tick::disable(Cpu_number cpu)
{
  _glbl_timer->chip()->mask_percpu(cpu, _glbl_timer->pin());
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
