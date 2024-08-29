/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

// ----------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n]:

#include "cpu.h"

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h"]
FIASCO_PURE unsigned
Platform_control::node_id()
{
  /*
   * MPIDR bitfield values:
   *   Aff0: 0..1 (core in cluster)
   *   Aff1: 0..1 (cluster in RTU)
   *   Aff2: 0..3 (RTU in SoC)
   */
  Mword mpidr = Cpu::mpidr();
  return (((mpidr & 0x100) >> 7)
            | (mpidr & 0x1))
            & 0x3;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32n]:

#include "kmem.h"
#include "koptions.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "panic.h"
#include "poll_timeout_counter.h"

IMPLEMENT_OVERRIDE
void
Platform_control::amp_ap_early_init()
{
  /* Setup core TCMs */
  static Mword const tcm_bases[4] = {
#if defined(CONFIG_PF_S32N_RTU_0)
    0x26000000, 0x26400000, 0x27000000, 0x27400000
#elif defined(CONFIG_PF_S32N_RTU_1)
    0x28000000, 0x28400000, 0x29000000, 0x29400000
#elif defined(CONFIG_PF_S32N_RTU_2)
    0x2A000000, 0x2A400000, 0x2B000000, 0x2B400000
#elif defined(CONFIG_PF_S32N_RTU_3)
    0x2C000000, 0x2C400000, 0x2D000000, 0x2D400000
#else
#error "Invalid RTU configuration"
#endif /* CONFIG_PF_S32N_RTU_0 */
  };
  Mword tcm_base = tcm_bases[node_id()];

  /* IMP_xTCMREGIONR. ENABLEEL2=1, ENABLEEL10=1 */
  asm volatile ("mcr p15, 0, %0, c9, c1, 0" : : "r"(tcm_base | 0x00000003));
  asm volatile ("mcr p15, 0, %0, c9, c1, 1" : : "r"(tcm_base | 0x00100003));
  asm volatile ("mcr p15, 0, %0, c9, c1, 2" : : "r"(tcm_base | 0x00200003));

  /* Split cache ways between AXIF and AXIM */
  /* IMP_CSCTLR.DFLW/IFLW. AXIF ways 0-1, AXIM ways 2-3 */
  asm volatile ("mcr p15, 1, %0, c9, c1, 0" : : "r"(0x202));
}

PUBLIC static void
Platform_control::amp_boot_init()
{
  Kip *k = Kip::k();

  /*
   * Need to always start on first core of the expected cluster. Otherwise the
   * GIC initialization or node memory assigment will fail.
   */
  assert(node_id() == Config::First_node);

  /* Alloc kernel memory of all AP cores. */
  unsigned long kmem[Config::Max_num_nodes - 1];
  for (unsigned i = 1; i < Config::Max_num_nodes; i++)
    {
      unsigned n = Config::First_node + i;
      if (k->sigma0[n])
        kmem[i - 1] = Kmem_alloc::reserve_kmem(n);
    }

  /* Boot AP CPUs. Do it serially because we have only one stack. */
  for (unsigned i = 1; i < Config::Max_num_nodes; i++)
    {
      unsigned n = Config::First_node + i;
      Cpu_boot_info &info = boot_info[i - 1];
      Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      if (!info.alive)
        {
          printf("AP CPU[%d] not alive!\n", n);
          continue;
        }

      info.kip = create_kip(kmem[i - 1], n);
      asm volatile ("dsb\n"
                    "sev");
      printf("Waiting for cpu %d ...", n);

      do
        Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      while (!info.booted);

      printf(" ok\n");
    }
}

/**
 * Redirect AP CPUs to out trampoline code.
 *
 * Should be done as early as possible to give the other CPUs a chance to
 * raise their "alive" flag.
 */
static void
setup_amp()
{
  Mword id = Platform_control::node_id();

  /* Redirect the other cores to our __amp_entry trampoline */
  if (id == Config::First_node && Koptions::o()->core_spin_addr != 0)
    {
      *((Mword *)Koptions::o()->core_spin_addr) = (Mword)&Platform_control::__amp_entry;
      Mem_unit::clean_dcache((void*)Koptions::o()->core_spin_addr);
      Mem::dsb();
      asm volatile ("sev");
    }
}
STATIC_INITIALIZER_P(setup_amp, BOOTSTRAP_INIT_PRIO);
