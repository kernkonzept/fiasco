IMPLEMENTATION [arm && mmu]:

#include "kmem.h"

union K_ptab_array
{
  Kpdir kpdir;
  Unsigned64 storage[512];
} __attribute__((aligned(0x1000)));

// initialize the kernel space (page table)
IMPLEMENT inline void Kmem_space::init() {}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && mmu && !cpu_virt]:

#include "boot_infos.h"

K_ptab_array kernel_l0_dir;
static K_ptab_array kernel_l0_vdir;

// Bootstrap will be able to use this many page tables under the top-level in
// order to map MMIO registers, pmem page directory and the kernel image into
// kernel_l0_vdir and the kernel image once again into kernel_l0_dir.
enum { Num_scratch_pages = 8 };
static K_ptab_array pdir_scratch[Num_scratch_pages];

DEFINE_GLOBAL_CONSTINIT Global_data<Kpdir *> Kmem::kdir(&kernel_l0_vdir.kpdir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_l0_dir.storage,
  kernel_l0_vdir.storage,
  pdir_scratch,
  (1 << Num_scratch_pages) - 1
};

// -----------------------------------------------------------------
IMPLEMENTATION [arm && mmu && cpu_virt]:

#include "boot_infos.h"

K_ptab_array kernel_l0_dir;
// Bootstrap will be able to use this many page tables under the top-level in
// order to map MMIO registers, pmem page directory and twice the kernel image
// into kernel_l0_dir.
enum { Num_scratch_pages = 8 };
static K_ptab_array pdir_scratch[Num_scratch_pages];

DEFINE_GLOBAL_CONSTINIT Global_data<Kpdir *> Kmem::kdir(&kernel_l0_dir.kpdir);

// Provide the initial information for bootstrap.cpp. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO _bs_pgin_dta =
{
  kernel_l0_dir.storage,
  pdir_scratch,
  (1 << Num_scratch_pages) - 1
};

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !mmu]:

IMPLEMENT inline
void Kmem_space::init()
{}
