IMPLEMENTATION [arm && mp && pf_s32g]:

EXTENSION class Platform_control
{
  enum { Num_cores = 4 };
};

IMPLEMENTATION [arm && mp && pf_s32g && !arm_psci]:

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "mmio_register_block.h"
#include "kmem.h"
#include "koptions.h"
#include "mem_unit.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  if (Koptions::o()->core_spin_addr == -1ULL)
    return;

  Mmio_register_block s(Kmem::mmio_remap(Koptions::o()->core_spin_addr,
                                         sizeof(Address)));
  s.r<Address>(0) = phys_tramp_mp_addr;
  Mem::dsb();
  Mem_unit::clean_dcache();

  for (int i = 1; i < min<int>(Num_cores, Config::Max_num_cpus); ++i)
    {
      asm volatile("sev" : : : "memory");
      while (!Cpu::online(Cpu_number(i)))
        {
          Mem::barrier();
          Proc::pause();
        }
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_s32g && arm_psci]:

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "psci.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  for (int i = 0; i < min<int>(Num_cores, Config::Max_num_cpus); ++i)
    {
      unsigned coreid[4] = { 0x0, 0x1, 0x100, 0x101 };
      int r = Psci::cpu_on(coreid[i], phys_tramp_mp_addr);
      if (r)
        {
          if (r != Psci::Psci_already_on)
            printf("CPU%d boot-up error: %d\n", i, r);
          continue;
        }

      while (!Cpu::online(Cpu_number(i)))
        {
          Mem::barrier();
          Proc::pause();
        }
    }
}
