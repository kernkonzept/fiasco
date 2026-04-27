IMPLEMENTATION [sparc]:
// ---------------------------------------------------------------------------
#include "std_macros.h"
#include "timer.h"
#include "processor.h"
#include "pic.h"

#error remove this file

IMPLEMENTATION:

#include "irq_chip_generic.h"

extern "C"
void irq_handler()
{
  Return_frame *rf = nonull_static_cast<Return_frame*>(current()->regs());

  Mword irq;

  if (rf->user_mode()) [[likely]]
    rf->srr1 = Proc::wake(rf->srr1);

  irq = Pic::pending();
  if (irq == Pic::No_irq_pending) [[unlikely]]
    return;

  Pic::main->handle_irq<Pic>(irq, nullptr);
}
