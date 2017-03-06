IMPLEMENTATION [arm]:

typedef Unsigned64 K_ptab_array[512] __attribute__((aligned(0x1000)));

// initialize the kernel space (page table)
IMPLEMENT inline void Kmem_space::init() {}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

#include "boot_infos.h"

K_ptab_array kernel_l0_dir;
static K_ptab_array kernel_l0_vdir;

// Bootstrap should be able to map up to 256TB RAM with six pages
enum { Num_scratch_pages = 6 };
static K_ptab_array pdir_scartch[Num_scratch_pages];

Kpdir *Mem_layout::kdir = (Kpdir *)&kernel_l0_vdir;

// provide the initial infos for bootstrap.cpp
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_l0_dir,
  kernel_l0_vdir,
  pdir_scartch,
  (1 << Num_scratch_pages) - 1
};

// -----------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "boot_infos.h"

K_ptab_array kernel_l0_dir;
// Bootstrap should be able to map up to 256TB RAM with six pages
enum { Num_scratch_pages = 6 };
static K_ptab_array pdir_scartch[Num_scratch_pages];

Kpdir *Mem_layout::kdir = (Kpdir *)&kernel_l0_dir;

// provide the initial infos for bootstrap.cpp
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_l0_dir,
  pdir_scartch,
  (1 << Num_scratch_pages) - 1
};
