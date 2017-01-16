IMPLEMENTATION [arm && mp]:

#include "io.h"
#include "platform_control.h"
#include "outer_cache.h"
#include "paging.h"

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

  _tmp->sctlr = Cpu::Sctlr_generic;
  _tmp->pdbr
    = Kmem::kdir->virt_to_phys((Address)Kmem::kdir) | Page::Ttbr_bits;
  _tmp->ttbcr   = Page::Ttbcr_bits;

  __asm__ __volatile__ ("" : : : "memory");
  Mem_unit::clean_dcache();

  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tmp->sctlr));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tmp->pdbr));
  Outer_cache::clean(Kmem::kdir->virt_to_phys((Address)&_tmp->ttbcr));

  Platform_control::boot_ap_cpus(Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry));
}

