IMPLEMENTATION [mips]:

#include "irq_mgr.h"

IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned glbl_irq)
{
  return Irq_mgr::mgr->attach(irq, glbl_irq);
}
