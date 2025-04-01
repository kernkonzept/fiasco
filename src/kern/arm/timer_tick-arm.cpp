IMPLEMENTATION [arm]:

#include "irq_mgr.h"


// Do the ARM specific way, really use an IRQ object and its hit
// function to handle timer IRQs as normal IRQs
IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned irqnum)
{ return Irq_mgr::mgr->attach(irq, irqnum, false); }

