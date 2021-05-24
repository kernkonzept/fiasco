IMPLEMENTATION [arm && mp && pf_fvp_base && !arm_psci]:

#include "cpu.h"
#include "mmio_register_block.h"
#include "kmem.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address)
{
  enum { PPONR = 4 };
  Mmio_register_block pwr(Kmem::mmio_remap(0x1c100000, 0x1000));

  unsigned coreid[7] = {          0x00100, 0x00200, 0x00300,
                         0x10000, 0x10100, 0x10200, 0x10300 };

  int seq = 1;
  for (int i = 0; i < 1; ++i)
    {
      pwr.r<32>(PPONR) = coreid[i];

      printf("Waiting for CPU%d[0x%x] to come up\n", seq, coreid[i]);
      while (!Cpu::online(Cpu_number(seq)))
        {
          Mem::barrier();
          Proc::pause();
        }

      seq++;
      if (seq == Config::Max_num_cpus)
        break;
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_fvp_base && arm_psci]:

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "psci.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  int seq = 1;
  for (int i = 0; i < min<int>(2, Config::Max_num_cpus); ++i)
    {
      unsigned coreid[16] = {
            0x000,   0x100,   0x200,   0x300,
            0x400,   0x500,   0x600,   0x700,
          0x10000, 0x10100, 0x10200, 0x10300,
          0x10400, 0x10500, 0x10600, 0x10700,
      };
      int r = Psci::cpu_on(coreid[i], phys_tramp_mp_addr);
      if (r)
        {
          if (r != Psci::Psci_already_on)
            printf("CPU%d boot-up error: %d\n", i, r);
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
