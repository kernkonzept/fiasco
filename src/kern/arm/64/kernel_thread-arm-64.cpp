IMPLEMENTATION [arm && mp && pic_gic]:

PRIVATE static inline NOEXPORT
void
Kernel_thread::boot_app_cpu_gic(Mp_boot_info volatile *inf)
{
  inf->gic_dist_base = Mem_layout::Gic_dist_phys_base;
  inf->gic_cpu_base = Mem_layout::Gic_cpu_phys_base;
}

IMPLEMENTATION [arm && mp && !pic_gic]:

PRIVATE static inline NOEXPORT
void
Kernel_thread::boot_app_cpu_gic(Mp_boot_info volatile *inf)
{
  inf->gic_dist_base = 0;
  inf->gic_cpu_base = 0;
}

IMPLEMENTATION [arm && mp]:

#include "io.h"
#include "platform_control.h"
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
    Mword gic_dist_base;
    Mword gic_cpu_base;
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
  boot_app_cpu_gic(_tmp);

  asm volatile ("dsb sy" : : : "memory");
  Mem_unit::clean_dcache();

  Platform_control::boot_ap_cpus(Kmem::kdir->virt_to_phys((Address)_tramp_mp_entry));
}
