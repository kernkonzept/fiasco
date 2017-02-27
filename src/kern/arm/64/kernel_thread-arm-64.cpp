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
    Mword tcr;
    Mword mair;
    Mword ttbr_kern;
    Mword ttbr_usr;
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
  Mp_boot_info volatile *_tmp;
  _tmp = reinterpret_cast<Mp_boot_info*>(_tramp_mp_boot_info);

  _tmp->sctlr = Proc::sctlr();
  _tmp->mair  = Page::Mair0_prrr_bits;
  _tmp->ttbr_kern = Kmem::kdir->virt_to_phys((Address)Kmem::kdir);
  if (!Proc::Is_hyp)
    _tmp->ttbr_usr = cxx::int_value<Phys_mem_addr>(Kernel_task::kernel_task()->dir_phys());

  _tmp->tcr   = Page::Ttbcr_bits;
  asm volatile ("dsb sy" : : : "memory");
  Mem_unit::clean_dcache();

  Platform_control::boot_ap_cpus(Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry));
}

