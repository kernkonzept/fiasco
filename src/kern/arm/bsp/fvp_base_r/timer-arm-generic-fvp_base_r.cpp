IMPLEMENTATION [arm_generic_timer && pf_fvp_base_r]:

#include "mem_layout.h"

PUBLIC static
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical:   return 30;
    case Generic_timer::Virtual:    return 27;
    case Generic_timer::Hyp:        return 26;
    case Generic_timer::Secure_hyp: return 20;
    };
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_fvp_base_r && 32bit]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
  asm volatile ("mcr p15, 0, %0, c14, c0, 0" : : "r"(100000000)); // set CNTFRQ
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_fvp_base_r && 64bit]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
  asm volatile ("msr CNTFRQ_EL0, %0" : : "r"(100000000)); // set CNTFRQ
}
