IMPLEMENTATION:

#include "entry_frame.h"
#include "processor.h"
#include "irq.h"
#include "pic.h"

extern "C"
void irq_handler()
{
  Return_frame *rf = nonull_static_cast<Return_frame*>(current()->regs());
  if (rf->user_mode()) [[likely]]
    rf->srr1 = Proc::wake(rf->srr1);

  Pic::main->post_pending_irqs();
}
