IMPLEMENTATION [arm]:

#include "kmem.h"

// always 16kB also for LPAE we use 4 consecutive second level tables
char kernel_page_directory[0x4000]
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
IMPLEMENTATION[arm && arm_lpae]:

#include "boot_infos.h"

Unsigned64 kernel_lpae_dir[4] __attribute__((aligned(4 * sizeof(Unsigned64))));
Kpdir *Kmem::kdir = reinterpret_cast<Kpdir *>(&kernel_lpae_dir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_page_directory,
  kernel_lpae_dir
};

//----------------------------------------------------------------------------------
IMPLEMENTATION[arm && !arm_lpae]:

#include "boot_infos.h"

Kpdir *Kmem::kdir = (Kpdir *)&kernel_page_directory;

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_page_directory
};
