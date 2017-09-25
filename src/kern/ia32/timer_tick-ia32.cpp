IMPLEMENTATION [ia32 || amd64 || ux]:

#include "config.h"
#include "irq_mgr.h"
#include "idt.h"

// On IA32 we do not use a real IRQ object but a special vector
IMPLEMENT bool
Timer_tick::allocate_irq(Irq_base *irq, unsigned irqnum)
{
  // we do not use the alloc function of the chip, because this would
  // actually route the IRQ vector through the IRQ object.
  // However, IA32 uses a vector that points to thread_timer_interrupt below,
  // bypassing the IRQ object infrastructure
  irqnum = Irq_mgr::mgr->legacy_override(irqnum);
  bool res = Irq_mgr::mgr->reserve(irqnum);
  if (res)
    {
      Irq_mgr::Irq i = Irq_mgr::mgr->chip(irqnum);
      i.chip->bind(irq, i.pin);

      // from now we can save energy in getchar()
      if (!Config::Scheduler_one_shot)
        Config::getchar_does_hlt_works_ok = false && Config::hlt_works_ok;
    }
  return res;
}

PUBLIC static
void
Timer_tick::set_vectors_stop()
{
  extern char entry_int_timer_stop[];
  // acknowledge timer interrupt once to keep timer interrupt alive because
  // we could be called from thread_timer_interrupt_slow() before ack
  Timer_tick::_glbl_timer->ack();

  // set timer interrupt to dummy doing nothing
  Idt::set_entry(Config::scheduler_irq_vector, (Address)entry_int_timer_stop, false);
#if 0
  // From ``8259A PROGRAMMABLE INTERRUPT CONTROLLER (8259A 8259A-2)'': If no
  // interrupt request is present at step 4 of either sequence (i. e. the
  // request was too short in duration) the 8259A will issue an interrupt
  // level 7. Both the vectoring bytes and the CAS lines will look like an
  // interrupt level 7 was requested.
  set_entry(0x27, (Address)entry_int_pic_ignore, false);
  set_entry(0x2f, (Address)entry_int_pic_ignore, false);
#endif
}

// We are entering with disabled interrupts!
extern "C" FIASCO_FASTCALL
void
thread_timer_interrupt(Address ip)
{
  //putchar('T');
  (void)ip;
  Timer_tick::handler_all(Timer_tick::_glbl_timer, 0);
}

/** Extra version of timer interrupt handler which is used when the jdb is
    active to prevent busy waiting. */
extern "C"
void
thread_timer_interrupt_stop(void)
{
  Timer_tick::_glbl_timer->ack();
}


