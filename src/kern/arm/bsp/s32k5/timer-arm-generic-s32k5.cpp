/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

IMPLEMENTATION [arm_generic_timer && pf_s32k5]:

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
IMPLEMENTATION [arm_generic_timer && pf_s32k5 && cpu_virt]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
  // Set CNTFRQ. CPE's generic timer is clocked from:
  // SAFE_CLK / CPE_GPR.CR52_CNT_DIV = 50MHz / 4 (default reset value)
  asm volatile ("mcr p15, 0, %0, c14, c0, 0" : : "r"(12500000));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_generic_timer && pf_s32k5 && !cpu_virt]:

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{ /* CNTFRQ can only be written on PL2. Assume that it's setup correctly. */ }
