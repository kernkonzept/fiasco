IMPLEMENTATION [arm && pf_armadaa8k]:

#include "minmax.h"
#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  for (int i = 0; i < min<int>(4, Config::Max_num_cpus); ++i)
    {
      int r;
      unsigned coreid[4] = { 0x000, 0x001, 0x100, 0x101 };
      if ((r = Psci::cpu_on(coreid[i], phys_tramp_mp_addr)))
        printf("KERNEL: PSCI CPU_ON for CPU%d failed: %d\n", i, r);
    }
}
