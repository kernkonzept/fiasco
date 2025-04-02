IMPLEMENTATION [arm && mp && (pf_rpi_rpi2 || pf_rpi_rpi3 || pf_rpi_rpi4) && !64bit]:

#include "arm_control.h"
#include "cpu.h"
#include "minmax.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Cpu_phys_id myid = Proc::cpu_id();
  int seq = 1;
  for (unsigned i = 0; i < min<unsigned>(4, Config::Max_num_cpus); ++i)
    if (myid != Cpu_phys_id(i))
      {
        Arm_control::o()->do_boot_cpu(Cpu_phys_id(i), phys_tramp_mp_addr);
        while (!Cpu::online(Cpu_number(seq)))
          {
            Mem::barrier();
            Proc::pause();
          }
        ++seq;
      }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && 64bit && (pf_rpi_rpi3 || pf_rpi_rpi4)]:

#include "cpu.h"
#include "kmem_mmio.h"
#include "minmax.h"
#include "mmio_register_block.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Mmio_register_block a(Kmem_mmio::map(0xd8, 0x28));
  Cpu_phys_id myid = Proc::cpu_id();
  int seq = 1;
  for (unsigned i = 0; i < min<unsigned>(4, Config::Max_num_cpus); ++i)
    if (myid != Cpu_phys_id(i))
      {
        a.r<64>(i * 8) = phys_tramp_mp_addr;
        Mem_unit::clean_dcache(reinterpret_cast<char *>(a.get_mmio_base())
                               + i * 8);
        asm volatile("sev");

        // All at once does not work, so wait one-by-one
        while (!Cpu::online(Cpu_number(seq)))
          {
            Mem::barrier();
            Proc::pause();
          }
        ++seq;
      }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && 64bit && pf_rpi_rpi5]:

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x000, 0x100, 0x200, 0x300 }, true);
}
