INTERFACE[ia32 || amd64]:

#include "std_macros.h"
#include "types.h"

IMPLEMENTATION[ia32 || amd64]:

#include "irq.h"
#include "logdefs.h"
#include "thread.h"

/**
 * Screen spinner for debugging purposes.
 */
static inline void irq_spinners([[maybe_unused]] Mword pin)
{
#ifdef CONFIG_IRQ_SPINNER
  Unsigned16 *cell
    = reinterpret_cast<Unsigned16 *>(Mem_layout::Adap_vram_cga_beg);
  cell += (20 + cxx::int_value<Cpu_number>(current_cpu())) * 80 + pin;

  if (cell < reinterpret_cast<Unsigned16 *>(Mem_layout::Adap_vram_cga_end))
    (*cell)++;
#endif
}

/**
 * Hardware interrupt entry point.
 *
 * Called with interrupts disabled. Pass execution to the corresponding
 * Irq_base::hit() method.
 *
 * \param irqobj  Hardware-interrupt object.
 */
extern "C" FIASCO_FASTCALL
void
irq_interrupt(Mword irqobj, Mword)
{
  Thread::assert_irq_entry();
  CNT_IRQ;

  Irq_base *irq = reinterpret_cast<Irq_base *>(irqobj);
  irq->log();
  irq_spinners(irq->pin());
  irq->hit(nullptr);
}
