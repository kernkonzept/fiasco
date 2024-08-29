/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

IMPLEMENTATION [arm_generic_timer && pf_s32n]:

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

IMPLEMENT
void Timer::bsp_init(Cpu_number)
{
  /* Set CNTFRQ. RTUs generic timer is clocked from FSS.SCTR clock */
  asm volatile ("mcr p15, 0, %0, c14, c0, 0" : : "r"(50000000));
}
