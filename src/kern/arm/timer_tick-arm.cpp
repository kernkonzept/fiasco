IMPLEMENTATION [arm]:

#include "irq_mgr.h"


// Do the ARM specific way, really use an IRQ object and its hit
// function to handle timer IRQs as normal IRQs
IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned gsi)
{ return Irq_mgr::mgr->gsi_attach(irq, gsi, false); }

