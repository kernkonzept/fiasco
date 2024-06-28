IMPLEMENTATION [arm && amp && pf_sr6p7g7]:

#include "cpu.h"
#include "kmem_mmio.h"
#include "koptions.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "panic.h"
#include "poll_timeout_counter.h"

enum : Unsigned32
{
  Me_gs = 0x000,
  Me_gs_s_mtrans = 1UL << 27,
  Me_mctl = 0x004,
  Me_mctl_key = 0x5af0,
  Me_mctl_key_inv = 0xa50f,
  Me_cctl0 = 0x1c6,
  Me_cctl1 = 0x1c4,
  Me_caddr0 = 0x1e0,
  Me_caddr1 = 0x1e4,
};

static bool
start_core(unsigned core, Address start_addr)
{
  static unsigned long const mc_me_base[3] = {
    0x710D0000, 0x716D0000, 0x710D4000
  };

  unsigned long base = mc_me_base[core >> 1];
  Mmio_register_block mc_me(Kmem_mmio::remap(base, 0x200));

  mc_me.write<Unsigned32>(start_addr | 1U, (core & 1U) ? Me_caddr1 : Me_caddr0);
  mc_me.write<Unsigned16>(0xfe, (core & 1U) ? Me_cctl1 : Me_cctl0);

  Unsigned32 mode = mc_me.read<Unsigned32>(Me_mctl) & 0xf0000000U;
  mc_me.write<Unsigned32>(mode | Me_mctl_key, Me_mctl);
  mc_me.write<Unsigned32>(mode | Me_mctl_key_inv, Me_mctl);

  L4::Poll_timeout_counter timeout(0x10000);
  while (timeout.test(mc_me.read<Unsigned32>(Me_gs) & Me_gs_s_mtrans));

  // Remove mappings to release precious MPU regions
  Kmem_mmio::unmap_compat(base, 0x200);

  return !timeout.timed_out();
}

// Trampoline because the cores will always start in thumb mode.
asm (
  ".text                          \n"
  ".thumb                         \n"
  ".balign 32                     \n" // Make sure it's usable as (H)VBAR
  ".globl __sr6p7_amp_entry       \n"
  "__sr6p7_amp_entry:             \n"
  "  ldr  r0, =__amp_main         \n"
  "  ldr  sp, =_stack             \n"
  "  bx   r0                      \n"
);

extern "C" void __sr6p7_amp_entry(void);

PUBLIC static void
Platform_control::amp_boot_init()
{
  // We assume that the first node boots the rest of the system
  assert(Amp_node::phys_id() == Amp_node::first_node());

  // Reserve memory for all AP CPUs up-front so that everybody sees the same
  // reserved regions.
  unsigned nodes = min<unsigned>(Amp_node::Max_cores, Amp_node::Max_num_nodes);
  for (unsigned i = 1; i < nodes; i++)
    Kmem_alloc::reserve_amp_heap(i);

  // Start cores serially because they all work on the same stack! No need to
  // flush the cache after updating boot_info. The R52 caches are always write
  // through.
  for (unsigned i = 1; i < nodes; i++)
    {
      Cpu_boot_info &info = boot_info[i - 1];

      if (!start_core(i, reinterpret_cast<Address>(&__sr6p7_amp_entry)))
        panic("Error starting core %u!\n", i);

      L4::Poll_timeout_counter guard(5000000);
      do
        Mem_unit::flush_dcache(&info, offset_cast<void *>(&info, sizeof(info)));
      while (guard.test(info != Cpu_boot_info::Booted));

      if (guard.timed_out())
        WARNX(Error, "CPU%d did not show up!\n", i);
    }
}
