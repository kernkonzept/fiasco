IMPLEMENTATION [arm && pf_bcm283x && pf_bcm283x_rpi1]:

#include "pic.h"

extern "C"
void irq_handler()
{ Pic::handle_irq(); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && (pf_bcm283x_rpi2 || pf_bcm283x_rpi3)]:

#include "ipi.h"
#include "thread.h"

inline NEEDS["ipi.h", "thread.h"]
void handle_ipis()
{
  Ipi::Message m = Ipi::pending();
  switch (m)
    {
    case Ipi::Request: Thread::handle_remote_requests_irq(); break;
    case Ipi::Global_request: Thread::handle_global_remote_requests_irq(); break;
    case Ipi::Debug: Thread::handle_debug_remote_requests_irq(); break;
    case Ipi::Timer: Thread::handle_timer_remote_requests_irq(0); break;
    };
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !mp && (pf_bcm283x_rpi2 || pf_bcm283x_rpi3)]:

inline void handle_ipis() {}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_bcm283x_rpi2 || pf_bcm283x_rpi3)]:

#include "pic.h"
#include "timer_tick.h"
#include "arm_control.h"

extern "C"
void irq_handler()
{
  while (Unsigned32 pending = Arm_control::o()->irqs_pending())
    {
      if (pending & (1 << 4)) // mailbox 0
        handle_ipis();

      if (pending & (1 << Timer::irq()))
        Timer_tick::handler_all(0, 0);

      if (pending & 0x100)
        Pic::handle_irq();
    }
}

