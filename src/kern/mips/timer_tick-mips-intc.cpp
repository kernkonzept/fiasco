IMPLEMENTATION [mips]:

#include "irq_mgr.h"

IMPLEMENT bool
Timer_tick::allocate_irq(Irq_base *irq, unsigned glbl_irq)
{
  return Irq_mgr::mgr->alloc(irq, glbl_irq);
}
