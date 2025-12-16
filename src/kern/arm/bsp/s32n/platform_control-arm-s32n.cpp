/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n && pf_s32n_rtu_0]:

EXTENSION class Platform_control
{
  static constexpr Mword tcm_bases[4] =
  { 0x26000000, 0x26400000, 0x27000000, 0x27400000 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n && pf_s32n_rtu_1]:

EXTENSION class Platform_control
{
  static constexpr Mword tcm_bases[4] =
  { 0x28000000, 0x28400000, 0x29000000, 0x29400000 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n && pf_s32n_rtu_2]:

EXTENSION class Platform_control
{
  static constexpr Mword tcm_bases[4] =
  { 0x2A000000, 0x2A400000, 0x2B000000, 0x2B400000 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n && pf_s32n_rtu_3]:

EXTENSION class Platform_control
{
  static constexpr Mword tcm_bases[4] =
  { 0x2C000000, 0x2C400000, 0x2D000000, 0x2D400000 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n]:

IMPLEMENT_OVERRIDE
void
Platform_control::amp_ap_early_init()
{
  // Setup core TCMs - IMP_xTCMREGIONR: ENABLEEL2=1, ENABLEEL10=1
  Mword tcm_base = tcm_bases[cxx::int_value<Amp_phys_id>(Amp_node::phys_id())];
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
