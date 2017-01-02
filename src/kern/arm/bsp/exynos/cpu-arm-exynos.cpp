IMPLEMENTATION [arm && bsp_cpu && arm_cache_l2cxx0]:

#include "platform.h"
#include "smc.h"

PRIVATE static
void
Cpu::do_cache_foz(unsigned en)
{
  Mword b = 1 << 3;
  if (Platform::running_ns())
    {
      Mword r;
      asm volatile("mrc p15, 0, %0, c1, c0, 1" : "=r" (r));
      Exynos_smc::write_cp15(0, 1, 0, 1, en ? (r | b) : (r & ~b));
    }
  else
    Cpu::modify_actrl(en ? b : 0, en ? 0 : b);
}

PUBLIC static
void
Cpu::enable_cache_foz()
{ do_cache_foz(1); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && bsp_cpu && !arm_cache_l2cxx0]:

PUBLIC static inline
void
Cpu::enable_cache_foz()
{}
