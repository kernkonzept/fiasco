INTERFACE[ia32 || amd64]:

#include "std_macros.h"
#include "types.h"

IMPLEMENTATION[ia32 || amd64]:

#include <cassert>

#include "cpu_lock.h"
#include "globalconfig.h"
#include "globals.h"
#include "irq.h"
#include "logdefs.h"
#include "std_macros.h"
#include "thread.h"
#include "timer.h"

// screen spinner for debugging purposes
static inline void irq_spinners(int irqnum)
{
#ifdef CONFIG_IRQ_SPINNER
  Unsigned16 *p = reinterpret_cast<Unsigned16 *>(Mem_layout::Adap_vram_cga_beg);
  p += (20 + cxx::int_value<Cpu_number>(current_cpu())) * 80 + irqnum;
  if (p < reinterpret_cast<Unsigned16 *>(Mem_layout::Adap_vram_cga_end))
    (*p)++;
#else
  (void)irqnum;
#endif
}

/** Hardware interrupt entry point.  Calls corresponding Dirq instance's
    Dirq::hit() method.
    @param irqobj hardware-interrupt object
 */
extern "C" FIASCO_FASTCALL
void
irq_interrupt(Mword irqobj, Mword ip)
{
  Thread::assert_irq_entry();

  CNT_IRQ;
  (void)ip;

  // we're entered with disabled irqs
  Irq_base *i = reinterpret_cast<Irq_base*>(irqobj);
  i->log();
  irq_spinners(i->pin());
  i->hit(nullptr);
}
