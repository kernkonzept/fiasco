IMPLEMENTATION [sparc]:

#include "irq_mgr.h"

IMPLEMENT bool
Timer_tick::allocate_irq(Irq_base *irq, unsigned irqnum)
{ return Irq_mgr::mgr->alloc(irq, irqnum); }
