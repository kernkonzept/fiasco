IMPLEMENTATION [arm && mp && pf_rcar4]:

#include "cpu.h"
#include "mem.h"
#include "psci.h"
#include "minmax.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  unsigned coreid[8] =
  { 0x00000, 0x00100, 0x10000, 0x10100, 0x20000, 0x20100, 0x30000, 0x30100 };

  int seq = 1;
  for (int i = 0; i < min<int>(8, Config::Max_num_cpus); ++i)
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
