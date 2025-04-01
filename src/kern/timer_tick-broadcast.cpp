INTERFACE:

#include "types.h"
#include "global_data.h"

EXTENSION class Timer_tick
{
public:
  static Global_data<Static_object<Timer_tick>> _glbl_timer;
};

IMPLEMENTATION:

#include "ipi.h"
#include "timer.h"

DEFINE_GLOBAL Global_data<Static_object<Timer_tick>> Timer_tick::_glbl_timer;

IMPLEMENT void
Timer_tick::setup(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      _glbl_timer.construct(Sys_cpu);
      if (!attach_irq(_glbl_timer, Timer::irq()))
        panic("Could not attach scheduling IRQ %d\n", Timer::irq());

      printf("Timer is at IRQ %d\n", Timer::irq());
      enable_vkey(cpu);

      _glbl_timer->chip()->set_mode(_glbl_timer->pin(), Timer::irq_mode());
    }
}

IMPLEMENT
void
Timer_tick::enable(Cpu_number)
{
  _glbl_timer->chip()->unmask(_glbl_timer->pin());
}

IMPLEMENT
void
Timer_tick::disable(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    _glbl_timer->chip()->mask(_glbl_timer->pin());
  else
    {
      // disable IPI
    }

}

PUBLIC inline NEEDS["timer.h", "ipi.h"]
void
Timer_tick::ack()
{
  Timer::acknowledge();
  Irq_base::ack();
  Ipi::bcast(Ipi::Timer, Cpu_number::boot_cpu());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

IMPLEMENT
Timer_tick *
Timer_tick::boot_cpu_timer_tick()
{ return _glbl_timer; }
