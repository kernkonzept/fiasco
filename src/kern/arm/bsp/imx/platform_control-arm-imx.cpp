INTERFACE [arm && mp && pf_imx_6]:

#include "types.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_imx_6]:

#include "ipi.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "kmem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Register_block<32> src(Kmem::mmio_remap(Mem_layout::Src_phys_base, 0x100));
  enum
  {
    SRC_SCR  = 0,
    SRC_GPR3 = 0x28,
    SRC_GPR5 = 0x30,
    SRC_GPR7 = 0x38,

    SRC_SCR_CORE1_3_ENABLE = 7 << 22,
    SRC_SCR_CORE1_3_RESET  = 7 << 14,
  };

  src[SRC_GPR3] = phys_tramp_mp_addr;
  src[SRC_GPR5] = phys_tramp_mp_addr;
  src[SRC_GPR7] = phys_tramp_mp_addr;

  src[SRC_SCR].set(SRC_SCR_CORE1_3_ENABLE | SRC_SCR_CORE1_3_RESET);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_imx_7 && !arm_psci]:

#include "ipi.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "kmem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  enum
  {
    GPC_CPU_PGC_SW_PUP_REQ             = 0x0f0,
    GPC_PGC_A7CORE1_CTRL               = 0x840,

    GPC_CPU_PGC_SW_PUP_REQ_CORE1_A7    = 1 << 1,
    GPC_PGC_A7CORE1_CTRL_PCR           = 1 << 0,

    SRC_A7RCR1                         = 0x08,
    SRC_GPR3                           = 0x7C,

    SRC_A7RCR_A7_CORE1_ENABLE          = 1 << 1,
  };

  Register_block<32> gpc(Kmem::mmio_remap(Mem_layout::Gpc_phys_base, 0x1000));
  Register_block<32> src(Kmem::mmio_remap(Mem_layout::Src_phys_base, 0x100));

  src[SRC_GPR3] = phys_tramp_mp_addr;

  gpc[GPC_PGC_A7CORE1_CTRL].set(GPC_PGC_A7CORE1_CTRL_PCR); // power off
  gpc[GPC_CPU_PGC_SW_PUP_REQ].set(GPC_CPU_PGC_SW_PUP_REQ_CORE1_A7); // power up 2nd core
  while (gpc[GPC_CPU_PGC_SW_PUP_REQ] & GPC_CPU_PGC_SW_PUP_REQ_CORE1_A7)
    ;
  gpc[GPC_PGC_A7CORE1_CTRL].clear(GPC_PGC_A7CORE1_CTRL_PCR); // enable again

  src[SRC_A7RCR1].set(SRC_A7RCR_A7_CORE1_ENABLE);
}


// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_imx_7 && arm_psci]:

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  if (cpu_on(0x1, phys_tramp_mp_addr))
    printf("KERNEL: PSCI CPU_ON failed\n");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && arm_v8 && arm_psci]:

#include "cpu.h"
#include "psci.h"
#include "minmax.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  int seq = 1;
  enum { Num_cores = 6 };
  unsigned coreid[Num_cores] = { 0x000, 0x001, 0x002, 0x003,
                                 0x100, 0x101 };

  for (int i = 0; i < min<int>(Num_cores, Config::Max_num_cpus); ++i)
    {
      int r = Psci::cpu_on(coreid[i], phys_tramp_mp_addr);
      if (r)
        {
          if (r != Psci::Psci_already_on)
            printf("KERNEL: CPU%d boot-up error: %d\n", i, r);
          continue;
        }

      while (!Cpu::online(Cpu_number(seq)))
        {
          Mem::barrier();
          Proc::pause();
        }
      ++seq;
    }
}

