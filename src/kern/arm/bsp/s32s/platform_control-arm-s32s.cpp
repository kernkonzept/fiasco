IMPLEMENTATION [arm && amp && pf_s32s]:

#include "cpu.h"
#include "kmem.h"
#include "koptions.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "panic.h"
#include "poll_timeout_counter.h"

IMPLEMENT_OVERRIDE
unsigned
Platform_control::node_id()
{
  Mword mpidr = Cpu::mpidr();
  return ((mpidr & 0x100U) >> 7) | (mpidr & 0x01);
}

static bool
start_core1(Mmio_register_block &mc_me,
            Mmio_register_block &mc_rgm,
            Address start_addr)
{
  enum
  {
    Ctl_key = 0x00,
    Prtn0_pconf = 0x100,
    Prtn0_stat = 0x108,
    Prtn0_core1_pconf = 0x160,
    Prtn0_core1_pupd = 0x164,
    Prtn0_core1_stat = 0x168,
    Prtn0_core1_addr = 0x16c,

    Prst_0 = 64,
    Pstat_0 = 320,
  };

  L4::Poll_timeout_counter timeout(0x10000);

  mc_me.write<Unsigned32>(1, Prtn0_core1_pconf);  // Enable the core clock
  mc_me.write<Unsigned32>(1, Prtn0_pconf);        // Enable the partition clock
  mc_me.write<Unsigned32>(1, Prtn0_core1_pupd);   // Trigger the hardware process
  mc_me.write<Unsigned32>(start_addr, Prtn0_core1_addr); // Set core pair 2 boot address

  mc_me.write<Unsigned32>(0x5AF0, Ctl_key);
  mc_me.write<Unsigned32>(0xA50F, Ctl_key);

  // Wait for WFI and CCS bits
  while (timeout.test(mc_me.read<Unsigned32>(Prtn0_core1_stat) != 0x80000001));
  while (timeout.test(!(mc_me.read<Unsigned32>(Prtn0_stat) & 0x1)));

  // Release the core reset via the corresponding MC_RGM_PRST register
  mc_rgm.write<Unsigned32>(mc_rgm.read<Unsigned32>(Prst_0) & 0xFFFFFFFC, Prst_0); //PERIPH_1 (core1)
  while (timeout.test(mc_rgm.read<Unsigned32>(Pstat_0) & 0x2));

  return !timeout.timed_out();
}

PUBLIC static void
Platform_control::amp_boot_init()
{
  enum
  {
    Gpr0 = 0x00,
  };

  // disable uart
  auto *ko = Koptions::o();
  ko->flags |= Koptions::F_noserial | Koptions::F_nojdb;
  ko->flags &= ~Koptions::F_serial_esc;

  // Alloc kernel memory of all AP cores
  boot_info[0].kip = create_kip(Kmem_alloc::reserve_kmem(1), 1);

  // Make sure CPUs start in ARM mode
  Mmio_register_block r52_cluster(Kmem::mmio_remap(Mem_layout::S32s_r52_cluster,
                                                   0x40));
  r52_cluster.write<Unsigned32>(0, Gpr0);

  // We only support the 2nd core in the first cluster.
  Mmio_register_block mc_me(Kmem::mmio_remap(Mem_layout::S32_mc_me, 0x400));
  Mmio_register_block mc_rgm(Kmem::mmio_remap(Mem_layout::S32_mc_rgm, 0x200));
  if (!start_core1(mc_me, mc_rgm, (Address)&__amp_entry))
    panic("Error starting core 1!\n");

  // Remove mappings to release precious MPU regions
  Kmem::mmio_unmap(Mem_layout::S32_mc_rgm, 0x200);
  Kmem::mmio_unmap(Mem_layout::S32_mc_me, 0x400);
  Kmem::mmio_unmap(Mem_layout::S32s_r52_cluster, 0x40);
}
