IMPLEMENTATION [arm && mmu]:

#include "kmem.h"

// always 16kB also for LPAE we use 4 consecutive second level tables
union
{
  Kpdir kpdir;
  char storage[0x4000];
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

union
{
  Kpdir kpdir;
  Unsigned64 storage[4];
} kernel_lpae_dir __attribute__((aligned(4 * sizeof(Unsigned64))));
Kpdir *Kmem::kdir = &kernel_lpae_dir.kpdir;

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

Kpdir *Kmem::kdir = &kernel_page_directory.kpdir;

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
