IMPLEMENTATION [arm && mp && (pf_bcm283x_rpi2 || pf_bcm283x_rpi3) && !64bit]:

#include "arm_control.h"
#include "cpu.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Cpu_phys_id myid = Proc::cpu_id();
  for (unsigned i = 0; i < 4; ++i)
    if (myid != Cpu_phys_id(i))
      Arm_control::o()->do_boot_cpu(Cpu_phys_id(i), phys_tramp_mp_addr);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_bcm283x_rpi3 && 64bit]:

#include "cpu.h"
#include "mmio_register_block.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  static Mmio_register_block a(Kmem::mmio_remap(0xd8));
  Cpu_phys_id myid = Proc::cpu_id();
  for (unsigned i = 0; i < 4; ++i)
    if (myid != Cpu_phys_id(i))
      {
        Mem_unit::clean_dcache();
        a.r<64>(i * 8) = phys_tramp_mp_addr;
        Mem::dsb();
        asm volatile("sev");
      }
}
