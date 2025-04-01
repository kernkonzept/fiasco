IMPLEMENTATION [ia32 || amd64]:

#include "config.h"
#include "irq_mgr.h"
#include "idt.h"

// On IA-32 and AMD64 we do not use an IRQ object but a special vector.
IMPLEMENT bool
Timer_tick::attach_irq(Irq_base *irq, unsigned isa_pin)
{
  // We do not use the attach() method of the IRQ chip, because that would
  // route the ISA IRQ pin through the IRQ object.
  // However, IA-32 and AMD64 uses a dedicated ISA vector that points to
  // the thread_timer_interrupt() method below, bypassing the IRQ object
  // infrastructure.

  Mword gsi = Irq_mgr::mgr->legacy_override(isa_pin);

  if (!Irq_mgr::mgr->gsi_reserve(gsi))
    return false;

  Irq_mgr::Chip_pin cp = Irq_mgr::mgr->chip_pin(gsi);
  cp.chip->bind(irq, cp.pin);

  return true;
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
  Idt::set_entry(Config::scheduler_irq_vector,
                 reinterpret_cast<Address>(entry_int_timer_stop), false);
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
