// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z]:

#include "cpu.h"

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h"]
FIASCO_PURE unsigned
Platform_control::node_id()
{
  Mword mpidr = Cpu::mpidr();

  // We know that Aff0 is in the range [0..3] and Aff1 in [0..1]...
  return ((mpidr >> 6) | mpidr) & 0x07;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32z]:

#include "kmem.h"
#include "koptions.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "panic.h"
#include "poll_timeout_counter.h"

IMPLEMENT_OVERRIDE
void
Platform_control::amp_ap_early_init()
{
  // Enable peripheral access through LLPP
  unsigned long imp_periphpregionr;
  asm volatile ("mrc p15, 0, %0, c15, c0, 0" : "=r"(imp_periphpregionr));
  imp_periphpregionr |= 3; // enable port @ EL2 and EL1/0
  asm volatile ("mcr p15, 0, %0, c15, c0, 0" : : "r"(imp_periphpregionr));

  // Setup TCM for each core. Already with ENABLEEL2 and ENABLEEL10 bits set.
  static Mword const tcm_bases[8] = {
    0x30000003, 0x30400003, 0x30800003, 0x30C00003,
    0x34000003, 0x34400003, 0x34800003, 0x34C00003
  };
  Mword tcm_base = tcm_bases[node_id()];
  asm volatile ("mcr p15, 0, %0, c9, c1, 0" : : "r"(tcm_base));
  asm volatile ("mcr p15, 0, %0, c9, c1, 1" : : "r"(tcm_base | 0x00100000));
  asm volatile ("mcr p15, 0, %0, c9, c1, 2" : : "r"(tcm_base | 0x00200000));

  // Split cache ways between AXIF and AXIM
  asm volatile ("mcr p15, 1, %0, c9, c1, 0" : : "r"(0x202));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32z && pf_s32z_amp_release]:

enum : Unsigned32
{
  // Mode Entry Module (MC_ME)
  Ctl_key = 0x00,

  Prtn1_core0 = 0x340,
  Prtn2_core0 = 0x540,

  Prtnx_corex_pconf = 0x0,
  Prtnx_corex_pupd = 0x4,
  Prtnx_corex_stat = 0x8,
  Prtnx_corex_addr = 0xc,

  // Reset Generation Module (MC_RGM)
  Prst1 = 0x48,
  Prst2 = 0x50,

  // RTU GRP
  Cfg_core = 0,
};

static bool
start_core(Mmio_register_block &mc_me,
           Mmio_register_block &mc_rgm,
           unsigned core, Address start_addr)
{
  L4::Poll_timeout_counter timeout(0x10000);

  Unsigned32 base = (core < 4) ? Prtn1_core0 : Prtn2_core0;
  base += 0x20U * (core & 0x3U);

  // Set start address
  mc_me.write<Unsigned32>(start_addr, base + Prtnx_corex_addr);

  // Enable clock to core
  mc_me.write<Unsigned32>(1, base + Prtnx_corex_pconf);  // Enable the core clock
  mc_me.write<Unsigned32>(1, base + Prtnx_corex_pupd);   // Trigger the hardware process
  mc_me.write<Unsigned32>(0x5AF0, Ctl_key);       // Kick hardware process
  mc_me.write<Unsigned32>(0xA50F, Ctl_key);
  while (timeout.test((mc_me.read<Unsigned32>(base + Prtnx_corex_stat) & 0x1U) == 0));

  // Releasing the reset of core
  base = (core < 4) ? Prst1 : Prst2;
  mc_rgm.write<Unsigned32>(mc_rgm.read<Unsigned32>(base)
                            & ~(1UL << ((core & 0x3) + 1)), base);

  return !timeout.timed_out();
}

PUBLIC static void
Platform_control::amp_boot_init()
{
  Kip *k = Kip::k();

  // Need to always start on first core of the expected cluster. Otherwise the
  // GIC initialization or node memory assigment will fail.
  assert(node_id() == Config::First_node);

  unsigned long kmem[Config::Max_num_nodes - 1];

  // Alloc kernel memory of all AP cores.
  for (unsigned i = 1; i < Config::Max_num_nodes; i++)
    {
      unsigned n = Config::First_node + i;
      if (k->sigma0[n])
        kmem[i - 1] = Kmem_alloc::reserve_kmem(n);
    }

  // Make sure CPUs start in ARM mode
  Mmio_register_block rtu0_grp(Kmem::mmio_remap(Mem_layout::Rtu0_gpr_base,
                                                0x100));
  rtu0_grp.write<Unsigned32>(rtu0_grp.read<Unsigned32>(Cfg_core) & ~4UL,
                             Cfg_core);
  Kmem::mmio_unmap(Mem_layout::Rtu0_gpr_base, 0x100);

  Mmio_register_block rtu1_grp(Kmem::mmio_remap(Mem_layout::Rtu1_gpr_base,
                                                0x100));
  rtu1_grp.write<Unsigned32>(rtu1_grp.read<Unsigned32>(Cfg_core) & ~4UL,
                             Cfg_core);
  Kmem::mmio_unmap(Mem_layout::Rtu1_gpr_base, 0x100);

  Mmio_register_block mc_me(Kmem::mmio_remap(Mem_layout::Mc_me_base, 0x800));
  Mmio_register_block mc_rgm(Kmem::mmio_remap(Mem_layout::Mc_rgm_base, 0x200));

  // Start cores serially because they all work on the same stack! No need to
  // flush the cache after updating boot_info. The R52 caches are always write
  // through.
  for (unsigned i = 1; i < Config::Max_num_nodes; i++)
    {
      unsigned n = Config::First_node + i;
      Cpu_boot_info &info = boot_info[i - 1];

      if (!k->sigma0[n])
        continue;
      info.kip = create_kip(kmem[i - 1], n);
      if (!start_core(mc_me, mc_rgm, n, (Address)&__amp_entry))
        panic("Error starting core %u!\n", n);

      printf("Waiting for cpu %u ...", n);
      do
        Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      while (!info.booted);

      printf(" ok\n");
    }

  // Remove mappings to release precious MPU regions
  Kmem::mmio_unmap(Mem_layout::Mc_rgm_base, 0x200);
  Kmem::mmio_unmap(Mem_layout::Mc_me_base, 0x800);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32z && pf_s32z_amp_spin_addr]:

PUBLIC static void
Platform_control::amp_boot_init()
{
  Kip *k = Kip::k();

  // Need to always start on first core of the expected cluster. Otherwise the
  // GIC initialization or node memory assigment will fail.
  assert(node_id() == Config::First_node);

  // Alloc kernel memory of all AP cores.
  unsigned long kmem[Config::Max_num_nodes - 1];
  for (unsigned i = 1; i < Config::Max_num_nodes; i++)
    {
      unsigned n = Config::First_node + i;
      if (k->sigma0[n])
        kmem[i - 1] = Kmem_alloc::reserve_kmem(n);
    }

  // Boot AP CPUs. Do ist serially because we have only one stack.
  for (unsigned i = 1; i < Config::Max_num_nodes; i++)
    {
      unsigned n = Config::First_node + i;
      Cpu_boot_info &info = boot_info[i - 1];
      Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      if (!info.alive)
        {
          printf("AP CPU[%d] not alive!\n", n);
          continue;
        }

      info.kip = create_kip(kmem[i - 1], n);
      asm volatile ("dsb\n"
                    "sev");
      printf("Waiting for cpu %d ...", n);

      do
        Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      while (!info.booted);

      printf(" ok\n");
    }
}

/**
 * Redirect AP CPUs to out trampoline code.
 *
 * Should be done as early as possible to give the other CPUs a chance to
 * raise their "alive" flag.
 */
static void
setup_amp()
{
  Mword id = Platform_control::node_id();

  // Redirect the other cores to our __amp_entry trampoline
  if (id == Config::First_node && Koptions::o()->core_spin_addr != 0)
    {
      *((Mword *)Koptions::o()->core_spin_addr) = (Mword)&Platform_control::__amp_entry;
      Mem_unit::clean_dcache((void*)Koptions::o()->core_spin_addr);
      Mem::dsb();
      asm volatile ("sev");
    }
}
STATIC_INITIALIZER_P(setup_amp, BOOTSTRAP_INIT_PRIO);
