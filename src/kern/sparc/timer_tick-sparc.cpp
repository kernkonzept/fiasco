IMPLEMENTATION [sparc]:

#include "irq_mgr.h"

IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned irqnum)
{ return Irq_mgr::mgr->attach(irq, irqnum); }
