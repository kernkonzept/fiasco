IMPLEMENTATION [mips]:

#include "irq_mgr.h"

IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned gsi)
{
  return Irq_mgr::mgr->gsi_attach(irq, gsi);
}
