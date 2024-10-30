INTERFACE [arm && pf_armada38x]:

#include "types.h"

IMPLEMENTATION [arm && pf_armada38x]:

#include "kmem_mmio.h"
#include "mmio_register_block.h"

IMPLEMENT_OVERRIDE
void
Platform_control::init(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  enum
  {
    Unit_sync_barrier_control_reg = 0x84,

    Control_off   = 0,
    Base_off      = 4,
    Remap_low_off = 8,
    Remap_hi_off  = 12,
  };
  Mmio_register_block cpu_subsys(Kmem_mmio::map(0xf1020000, 0x100));

  // Disable Window 0-7
  for (unsigned i = 0; i < 8; ++i)
    {
      cpu_subsys.r<32>(0x10 * i + Base_off) = 0;
      cpu_subsys.r<32>(0x10 * i + Control_off) = 0;
      cpu_subsys.r<32>(0x10 * i + Remap_low_off) = 0;
      cpu_subsys.r<32>(0x10 * i + Remap_hi_off) = 0;
    }

  // Disable Window 8-19
  for (unsigned i = 8; i < 20; ++i)
    {
      cpu_subsys.r<32>(0x90 + 8 * (i - 8) + Base_off) = 0;
      cpu_subsys.r<32>(0x90 + 8 * (i - 8) + Control_off) = 0;
    }
  // Window 13 Low+High Remap register
  cpu_subsys.r<32>(0xf0) = 0;
  cpu_subsys.r<32>(0xf4) = 0;

  cpu_subsys.r<32>(Unit_sync_barrier_control_reg) = 0xffff;

  cpu_subsys.r<32>(0x94) = 0xfff00000;
  cpu_subsys.r<32>(0x90) = 0x000f1d13;
}

IMPLEMENTATION [arm && mp && pf_armada38x]: // -------------------------------

#include "ipi.h"
#include "pic.h"
#include "mem.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  unsigned hwcpu = 1;
  // CPU1 Power Management
  Mmio_register_block pmu_c1(Kmem_mmio::map(0xf1022100 + hwcpu * 0x100,
                                            0x100));

  pmu_c1.r<32>(0x24) = phys_tramp_mp_addr;
  Mem::mp_wmb();
  Pic::gic->softint_phys(Ipi::Global_request, 1ul << (16 + hwcpu));

  // CPU0..n Software Reset Control Register
  Mmio_register_block cpu_reset(Kmem_mmio::map(0xf1020800, 8));
  cpu_reset.r<32>(hwcpu * 0x8).clear(1);
}
