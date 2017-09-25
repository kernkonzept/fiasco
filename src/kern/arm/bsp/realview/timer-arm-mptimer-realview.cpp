// --------------------------------------------------------------------------
IMPLEMENTATION[arm && mptimer]:

#include "platform.h"
#include "timer_sp804.h"

PRIVATE static Mword Timer::interval()
{
  Timer_sp804 timer(Kmem::mmio_remap(Mem_layout::Timer0_phys_base));
  Platform::system_control->modify<Mword>(Platform::System_control::Timer0_enable, 0, 0);

  Mword frequency = 1000000;
  Mword timer_start = ~0UL;
  unsigned factor = 5;
  Mword sp_c = timer_start - frequency / 1000 * (1 << factor);

  timer.disable();
  timer.counter_value(timer_start);
  timer.reload_value(timer_start);
  timer.enable(Timer_sp804::Ctrl_periodic);

  Mword vc = start_as_counter();
  while (sp_c < timer.counter())
    ;
  Mword interval = (vc - stop_counter()) >> factor;
  timer.disable();
  return interval;
}
