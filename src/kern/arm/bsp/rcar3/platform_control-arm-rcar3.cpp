IMPLEMENTATION [arm && mp && pf_rcar3]:

#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  for (int i = 1; i < 8; ++i)
    {
      unsigned coreid[8] = { 0x000, 0x001, 0x002, 0x003,
                             0x100, 0x101, 0x102, 0x103 };
      int r = cpu_on(coreid[i], phys_tramp_mp_addr);
      if (r && r != -2) // Different SoC variants
        printf("CPU%d boot-up error: %d\n", i, r);
    }
}
