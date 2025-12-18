/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32k5]:

#include "panic.h"

IMPLEMENT_OVERRIDE
void
Platform_control::amp_ap_early_init()
{
  // Enable LLPP peripheral port at EL2 and EL1/0
  unsigned long imp_periphpregionr;
  asm volatile ("mrc p15, 0, %0, c15, c0, 0" : "=r"(imp_periphpregionr));
  imp_periphpregionr |= 3;
  asm volatile ("mcr p15, 0, %0, c15, c0, 0" : : "r"(imp_periphpregionr));

  // Setup core TCMs
  Mword tcm_base = 0;
  if (Amp_node::phys_id() == Amp_phys_id{0})
    tcm_base = 0x01000000;
  else if (Amp_node::phys_id() == Amp_phys_id{1})
    tcm_base = 0x01400000;
  else
    panic("Running on invalid core!\n");

  // IMP_xTCMREGIONR. ENABLEEL2=1, ENABLEEL10=1
  asm volatile ("mcr p15, 0, %0, c9, c1, 0" : : "r"(tcm_base | 0x00000003));
  asm volatile ("mcr p15, 0, %0, c9, c1, 1" : : "r"(tcm_base | 0x00100003));
  asm volatile ("mcr p15, 0, %0, c9, c1, 2" : : "r"(tcm_base | 0x00200003));
}

PUBLIC static void
Platform_control::amp_boot_init()
{
  // Need to always start on first core of the expected cluster. Otherwise the
  // GIC initialization or node memory assignment will fail.
  assert(Amp_node::phys_id() == Amp_node::first_node());

  amp_boot_ap_cpus(Amp_node::Max_cores);
}

static void
setup_amp()
{ Platform_control::amp_prepare_ap_cpus(); }

STATIC_INITIALIZER_P(setup_amp, EARLY_INIT_PRIO);
