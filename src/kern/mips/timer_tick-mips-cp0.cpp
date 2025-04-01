IMPLEMENTATION [mips]:

#include "mips_cpu_irqs.h"

IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned cpu_irq)
{
  // ignore double alloc of the timer for CPU local IRQs
  if (   (irq->chip() == Mips_cpu_irqs::chip)
      && (irq->pin() == cpu_irq))
    return true;

  return Mips_cpu_irqs::chip->attach(irq, cpu_irq);
}
