IMPLEMENTATION [arm && mp && pf_rcar3]:

#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"
#include "psci.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  for (int i = 0; i < 8; ++i)
    {
      unsigned coreid[8] = { 0x000, 0x001, 0x002, 0x003,
                             0x100, 0x101, 0x102, 0x103 };
      int r = cpu_on(coreid[i], phys_tramp_mp_addr);
      if (r && r != Psci::Psci_invalid_parameters // Different SoC variants
          && r != Psci::Psci_already_on)
        printf("CPU%d boot-up error: %d\n", i, r);
    }
}
