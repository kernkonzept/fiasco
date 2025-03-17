IMPLEMENTATION [arm && mp && pf_s32g && !arm_psci]:

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"
#include "koptions.h"
#include "mem_unit.h"

EXTENSION class Platform_control
{
  enum { Num_cores = 4 }; // S32G3 still spinning?
};

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
  Mem_unit::clean_dcache(s.get_mmio_base());

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
IMPLEMENTATION [arm && mp && pf_s32g2 && arm_psci]:

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  boot_ap_cpus_psci(phys_tramp_mp_addr,
                    { 0x0, 0x1, 0x100, 0x101 });
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_s32g3 && arm_psci]:

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  boot_ap_cpus_psci(phys_tramp_mp_addr,
                    { 0x0, 0x1, 0x2, 0x3, 0x100, 0x101, 0x102, 0x103 });
}
