IMPLEMENTATION [arm && mp && pf_fvp_base]:

EXTENSION class Platform_control
{
  enum { Num_cores = 16 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base]:

#include "reset.h"

IMPLEMENT_OVERRIDE
void
Platform_control::system_off()
{
  platform_shutdown();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_fvp_base && !arm_psci]:

#include "cpu.h"
#include "kmem_mmio.h"
#include "koptions.h"
#include "mmio_register_block.h"
#include "warn.h"
#include "poll_timeout_counter.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  if (Koptions::o()->core_spin_addr == -1ULL)
    return;

  Mmio_register_block s(Kmem_mmio::map(Koptions::o()->core_spin_addr,
                                       sizeof(Address)));
  s.r<Address>(0) = phys_tramp_mp_addr;
  Mem::dsb();
  Mem_unit::clean_dcache();

  for (int i = 1; i < min<int>(Num_cores, Config::Max_num_cpus); ++i)
    {
      asm volatile("sev" : : : "memory");
      L4::Poll_timeout_counter guard(5000000);
      while (guard.test(!Cpu::online(Cpu_number(i))))
        {
          Mem::barrier();
          Proc::pause();
        }

      if (guard.timed_out())
        WARNX(Error, "CPU%d did not show up!\n", i);
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_fvp_base && arm_psci]:

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "psci.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  int seq = 1;
  for (int i = 0; i < min<int>(2, Config::Max_num_cpus); ++i)
    {
      unsigned coreid[Num_cores] = {
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
