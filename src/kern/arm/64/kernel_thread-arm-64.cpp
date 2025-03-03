IMPLEMENTATION [arm && mp && mmu && pic_gic && have_arm_gicv2]:

PRIVATE static inline NOEXPORT
void
Kernel_thread::boot_app_cpu_gic(Mp_boot_info volatile *inf)
{
  inf->gic_dist_base = Mem_layout::Gic_dist_phys_base;
  inf->gic_cpu_base = Mem_layout::Gic_cpu_phys_base;
}

IMPLEMENTATION [arm && mp && mmu && (!pic_gic || !have_arm_gicv2)]:

PRIVATE static inline NOEXPORT
void
Kernel_thread::boot_app_cpu_gic(Mp_boot_info volatile *inf)
{
  inf->gic_dist_base = 0;
}

IMPLEMENTATION [arm && mp && mmu]:

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
    Mword gic_dist_base; // only needed for IGROUPR0
    Mword gic_cpu_base;
  };
};

PUBLIC
static void
Kernel_thread::boot_app_cpus()
{
  extern char _tramp_mp_entry[];
  extern char _tramp_mp_boot_info[];
  Mp_boot_info *_tmp;
  _tmp = reinterpret_cast<Mp_boot_info*>(_tramp_mp_boot_info);

  _tmp->sctlr = Proc::sctlr();
  _tmp->mair  = Page::Mair0_prrr_bits;
  _tmp->ttbr_kern
    = Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(Kmem::kdir.unwrap()));
  if constexpr (!Proc::Is_hyp)
    _tmp->ttbr_usr = cxx::int_value<Phys_mem_addr>(Kernel_task::kernel_task()->dir_phys());

  _tmp->tcr   = Page::Ttbcr_bits;
  boot_app_cpu_gic(_tmp);

  asm volatile ("dsb sy" : : : "memory");
  Mem_unit::clean_dcache(_tmp, _tmp + 1);

  Platform_control::boot_ap_cpus(
    Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(_tramp_mp_entry)));
}

//------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && mpu]:

#include "kmem.h"
#include "mem_unit.h"
#include "platform_control.h"

EXTENSION class Kernel_thread
{
  struct Mp_boot_info
  {
    Mword sctlr;
    Mword mair;
    Mword prbar0;
    Mword prlar0;
  };
};

PUBLIC
static void
Kernel_thread::boot_app_cpus()
{
  extern char _tramp_mp_boot_info[];
  Mp_boot_info *_tmp;
  _tmp = reinterpret_cast<Mp_boot_info*>(_tramp_mp_boot_info);

  _tmp->sctlr = Proc::sctlr();
  _tmp->mair  = Mpu::Mair_bits;
  _tmp->prbar0 = (*Kmem::kdir)[Kpdir::Kernel_text].prbar;
  _tmp->prlar0 = (*Kmem::kdir)[Kpdir::Kernel_text].prlar;

  asm volatile ("dsb sy" : : : "memory");
  Mem_unit::clean_dcache(_tmp, _tmp + 1);

  extern char _tramp_mp_entry[];
  Platform_control::boot_ap_cpus(reinterpret_cast<Address>(_tramp_mp_entry));
}
