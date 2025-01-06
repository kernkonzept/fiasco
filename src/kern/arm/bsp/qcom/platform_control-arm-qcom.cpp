/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 */

IMPLEMENTATION [arm && mp && arm_psci && (pf_qcom_msm8909 || pf_qcom_msm8916)]:
static unsigned const psci_coreid[] =
{ 0x0, 0x1, 0x2, 0x3 };

IMPLEMENTATION [arm && mp && arm_psci && pf_qcom_msm8939]:
static unsigned const psci_coreid[] =
{ 0x000, 0x001, 0x002, 0x003, 0x100, 0x101, 0x102, 0x103 };

IMPLEMENTATION [arm && mp && arm_psci && pf_qcom_sm8150]:
static unsigned const psci_coreid[] =
{ 0x000, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700 };

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_qcom && arm_psci]:

#include <cstdio>

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "psci.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  unsigned ncpus = min<unsigned>(cxx::size(psci_coreid), Config::Max_num_cpus);
  for (unsigned i = 0; i < ncpus; ++i)
    {
      int r = Psci::cpu_on(psci_coreid[i], phys_tramp_mp_addr);
      if (r && r != Psci::Psci_already_on)
        printf("CPU%d boot-up error: %d\n", i, r);
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_qcom && !arm_psci]:

#include "io.h"
#include "kmem_mmio.h"
#include "koptions.h"
#include "mem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  if (Koptions::o()->core_spin_addr == -1ULL)
    return;

  void *base = Kmem_mmio::map(Koptions::o()->core_spin_addr, sizeof(Address));
  Io::write<Address>(phys_tramp_mp_addr, base);
  Mem::dsb();
  asm volatile("sev" : : : "memory");
}
