// --------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

#include "config.h"
#include "kmem.h"

namespace Boot_paging
{
  // always 16 KB also for LPAE we use 4 consecutive second level tables
  constexpr unsigned Kernel_pdir_size = 4 * Config::PAGE_SIZE;
}

union
{
  Kpdir kpdir;
  Unsigned8 storage[Boot_paging::Kernel_pdir_size];
} kernel_page_directory
  __attribute__((aligned(0x4000), section(".bss.kernel_page_dir")));

// initialize the kernel space (page table)
IMPLEMENT
void Kmem_space::init()
{
  unsigned domains = 0x0001;

  asm volatile("mcr p15, 0, %0, c3, c0" : : "r" (domains));

  Mem_unit::clean_vdcache();
}

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && mmu && arm_lpae]:

#include "boot_infos.h"

namespace Boot_paging
{
  constexpr unsigned Kernel_lpae_dir_size = sizeof(Unsigned64) * 4;
}

union
{
  Kpdir kpdir;
  Unsigned8 storage[Boot_paging::Kernel_lpae_dir_size];
} kernel_lpae_dir __attribute__((aligned(4 * sizeof(Unsigned64))));

DEFINE_GLOBAL_CONSTINIT Global_data<Kpdir *> Kmem::kdir(&kernel_lpae_dir.kpdir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_page_directory.storage,
  kernel_lpae_dir.storage
};

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && mmu && !arm_lpae]:

#include "boot_infos.h"

DEFINE_GLOBAL_CONSTINIT
Global_data<Kpdir *> Kmem::kdir(&kernel_page_directory.kpdir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_page_directory.storage
};

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && !mmu]:

IMPLEMENT inline
void Kmem_space::init()
{}
