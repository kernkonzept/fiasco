IMPLEMENTATION [arm && mp && pf_bcm283x]: // ------------------------------

#include "arm_control.h"
#include "cpu.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Cpu_phys_id myid = Proc::cpu_id();
  for (unsigned i = 0; i < 4; ++i)
    if (myid != Cpu_phys_id(i))
      Arm_control::boot_cpu(Cpu_phys_id(i), phys_tramp_mp_addr);
}
