IMPLEMENTATION [arm_generic_timer && pf_sr6p7g7]:

#include "mem_layout.h"

PUBLIC static
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

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_sr6p7g7 && cpu_virt]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
  asm volatile ("mcr p15, 0, %0, c14, c0, 0" : : "r"(1000000000)); // set CNTFRQ
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_sr6p7g7 && !cpu_virt]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{ /* CNTFRQ can only be written on PL2. Assume that it's setup correctly. */ }
