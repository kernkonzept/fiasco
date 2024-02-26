IMPLEMENTATION [arm]:

#include "kmem.h"

typedef Unsigned64 K_ptab_array[512] __attribute__((aligned(0x1000)));

// initialize the kernel space (page table)
IMPLEMENT inline void Kmem_space::init() {}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

#include "boot_infos.h"

K_ptab_array kernel_l0_dir;
static K_ptab_array kernel_l0_vdir;

// Bootstrap will be able to use this many page tables under the top-level in
// order to map MMIO registers, pmem page directory and the kernel image into
// kernel_l0_vdir and the kernel image once again into kernel_l0_dir.
enum { Num_scratch_pages = 8 };
static K_ptab_array pdir_scratch[Num_scratch_pages];

Kpdir *Kmem::kdir = reinterpret_cast<Kpdir *>(&kernel_l0_vdir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_l0_dir,
  kernel_l0_vdir,
  pdir_scratch,
  (1 << Num_scratch_pages) - 1
};

// -----------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "boot_infos.h"

K_ptab_array kernel_l0_dir;
// Bootstrap will be able to use this many page tables under the top-level in
// order to map MMIO registers, pmem page directory and twice the kernel image
// into kernel_l0_dir.
enum { Num_scratch_pages = 8 };
static K_ptab_array pdir_scratch[Num_scratch_pages];

Kpdir *Kmem::kdir = reinterpret_cast<Kpdir *>(&kernel_l0_dir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_l0_dir,
  pdir_scratch,
  (1 << Num_scratch_pages) - 1
};
