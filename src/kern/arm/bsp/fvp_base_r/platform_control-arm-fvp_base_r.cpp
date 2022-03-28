// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && amp]:

#include <string.h>
#include "koptions.h"
#include "mem_unit.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "cpu.h"
#include "kip.h"
#include "construction.h"

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h"]
unsigned
Platform_control::node_id()
{
  return Cpu::mpidr() & 0xffU;
}

PUBLIC static void
Platform_control::amp_boot_init()
{
  auto *ko = Koptions::o();

  // Reserve memory for all AP CPUs up-front so that everybody sees the same
  // reserved regions.
  unsigned long kmem[Config::Max_num_nodes - 1];
  for (int i = 0; i < Config::Max_num_nodes - 1; i++)
    kmem[i] = Kmem_alloc::reserve_kmem(i + 1);

  // Boot AP CPUs. Do ist serially because the Koptions are adapted for each
  // CPU individually.
  for (int i = 0; i < Config::Max_num_nodes - 1; i++)
    {
      Cpu_boot_info &info = boot_info[i];
      Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      if (!info.alive)
        {
          printf("AP CPU[%d] not alive!\n", i + 1);
          continue;
        }

      // next uart
      ko->uart.irqno++;
      ko->uart.base_address += 0x10000;
      Mem_unit::clean_dcache(ko, ko+1);

      info.kip = create_kip(kmem[i], i+1);
      Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
      asm volatile ("sev");
      printf("Waiting for cpu %d ...", i + 1);

      do
        {
          asm volatile ("wfe");
          Mem_unit::flush_dcache(&info, (char *)&info + sizeof(info));
        }
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
  if (id == 0 && Koptions::o()->core_spin_addr != 0)
    {
      *((Mword *)Koptions::o()->core_spin_addr) = (Mword)&Platform_control::__amp_entry;
      Mem_unit::clean_dcache((void*)Koptions::o()->core_spin_addr);
      Mem::dsb();
      asm volatile ("sev");
    }
}
STATIC_INITIALIZER_P(setup_amp, BOOTSTRAP_INIT_PRIO);

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && mp]:

#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "mmio_register_block.h"
#include "kmem.h"
#include "koptions.h"
#include "mem_unit.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Mmio_register_block s(Kmem::mmio_remap(Koptions::o()->core_spin_addr, 8));

  // release all cores
  s.r<sizeof(Address)*8>(0) = phys_tramp_mp_addr;
  Mem_unit::clean_dcache((void *)s.get_mmio_base());
  Mem::dsb();
  asm volatile("sev");

  // Remove mappings to release precious MPU regions
  Kmem::mmio_unmap(Koptions::o()->core_spin_addr, 8);
}
