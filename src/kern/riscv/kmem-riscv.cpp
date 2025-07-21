INTERFACE [riscv]:

#include "mem_layout.h"
#include "paging.h"

// Number of statically allocated pages for the kernel bootstrap page tables.
// The bootstrap page tables provide an identity mapping of the kernel, a
// virtual mapping of the kernel, an MMIO page directory, and a mapping of the
// kernel's pmem. All the above are mapped as superpages. Depending on the
// page table layout, in particular on the number of levels, different numbers
// of page tables are needed.
IMPLEMENTATION [riscv && 32bit]:
enum { Num_boot_pages = 1 };

IMPLEMENTATION [riscv && riscv_sv39]:
// 1 x Root page table
// + 1 x L1 for identity mapping of kernel
// + 1 x L1 for virtual mapping of kernel
// + 1 x L1 for MMIO page directory
// + 4 x L1 for pmem (up to 4 GB)
enum { Num_boot_pages = 8 };

IMPLEMENTATION [riscv && riscv_sv48]:
// 1 x Root page table
// + 1 x L1 + 1 x L2 for identity mapping of kernel
// + 1 x L1 + 1 x L2 for virtual mapping of kernel
// + 1 x L1 + 1 x L2 for MMIO page directory
// + 1 x L1 + 4 x L2 for pmem (up to 4 GB)
enum { Num_boot_pages = 12 };

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "boot_infos.h"
#include "config.h"
#include "cpu.h"
#include "kip.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "panic.h"
#include "ram_quota.h"
#include "paging_bits.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// The memory for page tables has to be page aligned, however we can not use the
// aligned attribute to ensure proper alignment, because that would propagate to
// the alignment of the .bss section, preventing the linker from performing
// relaxation against the global pointer.
// Instead, we allocate memory for one more page than we actually need and do
// the alignment ourselves in Boot_paging_info.
static Unsigned8 boot_page_memory[Pg::size(Num_boot_pages + 1)]
  __attribute__((section(".bss.boot_page_memory")));

// Provide memory for the paging bootstrap mechanism. The kernel linker script
// overlays the Boot_paging_info member variable in Bootstrap_info with this.
static Boot_paging_info FIASCO_BOOT_PAGING_INFO
  _bs_pgin_dta(&boot_page_memory, Num_boot_pages);

// This pointer indirection is needed because in all of RISC-V's code models,
// statically defined symbols must lie within a single 2 GiB address range.
// For _bs_pgin_dta this condition does not hold, as it is put into the
// bootstrap.info section. Therefore, linking of code that directly accesses
// _bs_pgin_dta would fail with a "relocation truncated to fit" error.
static Boot_paging_info *bs_pgin_dta __attribute__((used)) = &_bs_pgin_dta;

DEFINE_GLOBAL_CONSTINIT Global_data<Kpdir *> Kmem::kdir;


IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword /*cause*/)
{
  return in_kernel(pfa);
}

PUBLIC static
void
Kmem::init()
{
  Mem_unit::init_asids();
  Mem_unit::init_vmids();
}

PUBLIC static
void
Kmem::init_paging()
{
  // So far the kernel page tables have been allocated from boot_page_memory.
  // Reallocate them now in pmem, as pmem_to_phys() would return incorrect
  // results for page tables located in boot_page_memory.
  auto alloc = Kmem_alloc::q_allocator(Ram_quota::root.unwrap());
  kdir = static_cast<Kpdir*>(alloc.alloc(Bytes(sizeof(Kpdir))));
  memset(kdir, 0, sizeof(Kpdir));

  // Map kernel image.
  if (!kdir->map(boot_virt_to_phys(Virt_addr(Mem_layout::Map_base)),
                 Virt_addr(Mem_layout::Map_base),
                 Virt_size(bs_pgin_dta->kernel_image_size()),
                 Page::Attr::kern_global(Page::Rights::RWX()),
                 Kpdir::Super_level, false, alloc))
    panic("Failed to map kernel image.");

  // Sync Pmem.
  sync_from_boot_kdir(kdir, alloc,
                      Mem_layout::Pmem_start, Mem_layout::Pmem_end);

  // Sync MMIO page directory
  sync_from_boot_kdir(kdir, alloc,
                      Mem_layout::Mmio_map_start, Mem_layout::Mmio_map_end);

  // Switch to new page table
  Cpu::set_satp(Mem_unit::Asid_boot, Cpu::phys_to_ppn(pmem_to_phys(kdir)));
  // Full tlb flush as global mappings have been changed.
  Mem_unit::tlb_flush();
}

PUBLIC static
void
Kmem::init_ap_paging()
{
  Cpu::set_satp(Mem_unit::Asid_boot, Cpu::phys_to_ppn(pmem_to_phys(kdir)));
  // Full tlb flush as global mappings have been changed.
  Mem_unit::tlb_flush();
}

PUBLIC static
bool
Kmem::boot_map_pmem(Address phys, Mword size)
{
  if (!Super_pg::aligned(phys) || !Super_pg::aligned(size))
    panic("Pmem must be superpage aligned!");

  Mem_layout::pmem_phys_base(phys);

  if (!boot_kdir_map(phys, Virt_addr(Mem_layout::Pmem_start), Virt_size(size),
                     Page::Attr::space_local(Page::Rights::RW()),
                     Kpdir::Super_level))
    return false;

  Mem_unit::tlb_flush(Mem_unit::Asid_boot);

  return true;
}

/**
 * Looks up a virtual address in the boot page table.
 */
PRIVATE static
Address
Kmem::boot_virt_to_phys(Virt_addr virt)
{
  auto i = boot_kdir_walk(virt, Kpdir::Super_level);
  if (!i.is_valid())
    return ~0;

  return i.page_addr() | cxx::get_lsb(cxx::int_value<Virt_addr>(virt),
                                      i.page_order());
}

PRIVATE static
Kpdir *
Kmem::boot_kdir()
{
  return static_cast<Kpdir *>(
    static_cast<void *>(bs_pgin_dta->kernel_page_directory()));
}

PUBLIC static
Pte_ptr
Kmem::boot_kdir_walk(Virt_addr virt, unsigned level)
{
  return boot_kdir()->walk(virt, level, bs_pgin_dta->mem_map());
}

PUBLIC [[nodiscard]] static
bool
Kmem::boot_kdir_map(Address phys, Virt_addr virt, Virt_size size,
                    Page::Attr attr, unsigned level)
{
  auto alloc = bs_pgin_dta->alloc_virt(&Kmem::boot_virt_to_phys);
  auto mem_map = bs_pgin_dta->mem_map();
  return boot_kdir()->map(phys, virt, size, attr, level, false, alloc, mem_map);
}

/**
 * Syncs a range of superpages from the boot kdir to kdir.
 */
PRIVATE template<typename ALLOC> static
void
Kmem::sync_from_boot_kdir(Kpdir *kdir, ALLOC const &alloc,
                          Address start, Address end)
{
  for (Address addr = start; addr < end; addr += Config::SUPERPAGE_SIZE)
    {
      auto be = boot_kdir_walk(Virt_addr(addr), Kpdir::Super_level);
      if (be.is_valid())
        {
          auto e = kdir->walk(Virt_addr(addr), Kpdir::Super_level, false, alloc);
          e.set_page(be.entry());
        }
    }
}
