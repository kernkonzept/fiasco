IMPLEMENTATION [mips]:

#include "mips_cpu_irqs.h"

IMPLEMENT bool
Timer_tick::allocate_irq(Irq_base *irq, unsigned cpu_irq)
{
  // ignore double alloc of the timer for CPU local IRQs
  if (   (irq->chip() == Mips_cpu_irqs::chip)
      && (irq->pin() == cpu_irq))
    return true;

  return Mips_cpu_irqs::chip->alloc(irq, cpu_irq);
}
