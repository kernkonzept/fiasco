IMPLEMENTATION [arm_generic_timer && pf_rcar4_r52]:

#include "mem_layout.h"

PUBLIC static inline
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 30;
    case Generic_timer::Virtual:  return 27;
    case Generic_timer::Secure_hyp:
    case Generic_timer::Hyp:      return 26;
    };
}

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{}
