IMPLEMENTATION [arm && mp]:

#include "io.h"
#include "platform_control.h"
#include "outer_cache.h"
#include "scu.h"
#include "paging.h"
#include <cstdio>

EXTENSION class Kernel_thread
{
  struct Mp_boot_info
  {
    Mword sctlr;
    Mword pdbr;
    Mword ttbcr;
  };
};

PUBLIC
static void
Kernel_thread::boot_app_cpus()
{
  if (Config::Max_num_cpus <= 1)
    return;

  extern char _tramp_mp_entry[];
  extern char _tramp_mp_boot_info[];
  Mp_boot_info volatile *_tmp = reinterpret_cast<Mp_boot_info*>(_tramp_mp_boot_info);

  if (Scu::Available)
    {
      unsigned num_ap_cpus = Cpu::scu->config() & 3;
      printf("Number of CPUs: %d\n", num_ap_cpus + 1);
    }

  _tmp->sctlr = Cpu::Sctlr_generic;
  _tmp->pdbr
    = Kmem_space::kdir()->virt_to_phys((Address)Kmem_space::kdir()) | Page::Ttbr_bits;
  _tmp->ttbcr   = Page::Ttbcr_bits;

  __asm__ __volatile__ ("" : : : "memory");
  Mem_unit::clean_dcache();

  Outer_cache::clean(Kmem_space::kdir()->virt_to_phys((Address)&_tmp->sctlr));
  Outer_cache::clean(Kmem_space::kdir()->virt_to_phys((Address)&_tmp->pdbr));
  Outer_cache::clean(Kmem_space::kdir()->virt_to_phys((Address)&_tmp->ttbcr));

  Platform_control::boot_ap_cpus(Kmem_space::kdir()->virt_to_phys((Address)_tramp_mp_entry));
}

