IMPLEMENTATION [ia32 || amd64]:

#include "apic.h"
#include "idt.h"

IMPLEMENT
void
Timer_tick::setup(Cpu_number cpu)
{
  enable_vkey(cpu);
}

IMPLEMENT
void
Timer_tick::enable(Cpu_number)
{
  Apic::timer_enable_irq();
  Apic::irq_ack();
}

IMPLEMENT
void
Timer_tick::disable(Cpu_number)
{
  Apic::timer_disable_irq();
}

PUBLIC static inline NEEDS["apic.h"]
void
Timer_tick::ack()
{
  Apic::irq_ack();
}

PUBLIC static
void
Timer_tick::set_vectors_stop()
{
  extern char entry_int_timer_stop[];
  // acknowledge timer interrupt once to keep timer interrupt alive because
  // we could be called from thread_timer_interrupt_slow() before ack
  Apic::irq_ack();

  // set timer interrupt to dummy doing nothing
  Idt::set_entry(Config::scheduler_irq_vector,
                 reinterpret_cast<Address>(entry_int_timer_stop), false);
}

// We are entering with disabled interrupts!
extern "C" FIASCO_FASTCALL
void
thread_timer_interrupt(Address ip)
{
  (void)ip;
  Timer_tick::ack();
  Timer_tick::handler_all_noack();
}

/** Extra version of timer interrupt handler which is used when the jdb is
    active to prevent busy waiting. */
extern "C"
void
thread_timer_interrupt_stop(void)
{
  Apic::irq_ack();
}
