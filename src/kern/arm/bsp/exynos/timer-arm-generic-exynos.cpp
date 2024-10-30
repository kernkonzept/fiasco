IMPLEMENTATION [arm_generic_timer && pf_exynos]:

#include "timer_mct.h"
#include "mem_layout.h"

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 29; // we use this mode in TZ secure mode (so sec IRQ)
    case Generic_timer::Virtual:  return 27;
    case Generic_timer::Hyp:      return 26;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
  Mct_timer mct(Kmem_mmio::map(Mem_layout::Mct_phys_base, 0x100));
  mct.start_free_running();
}
