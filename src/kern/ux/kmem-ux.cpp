INTERFACE [ux]:

class Kernel_task;

EXTENSION class Kmem
{
protected:
  static Pdir *kdir;	///< Kernel page directory

  friend class Kernel_task;
};

IMPLEMENTATION [ux]:

#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>

#include "boot_info.h"
#include "kmem_alloc.h"
#include "emulation.h"


IMPLEMENT inline Mword Kmem::is_io_bitmap_page_fault(Address)
{ return false; }

IMPLEMENT inline
Mword
Kmem::is_ipc_page_fault(Address addr, Mword error)
{ return addr <= User_max && (error & PF_ERR_REMTADDR); }

IMPLEMENT inline NEEDS ["regdefs.h"]
Mword
Kmem::is_kmem_page_fault(Address addr, Mword error)
{ return !(addr <= User_max && (error & PF_ERR_USERADDR)); }

/**
 * Compute physical address from a kernel-virtual address.
 * @param addr a virtual address
 * @return corresponding physical address if a mappings exists.
 *         -1 otherwise.
 */
IMPLEMENT inline NEEDS["paging.h","std_macros.h"]
Address
Kmem::virt_to_phys (const void *addr)
{
  Address a = reinterpret_cast<Address>(addr);

  if (EXPECT_TRUE(Mem_layout::in_pmem(a)))
    return Mem_layout::pmem_to_phys(a);

  return kdir->virt_to_phys(a);
}

PUBLIC static inline 
Address Kmem::kernel_image_start()
{ return Mem_layout::Kernel_start_frame; }

IMPLEMENT inline Address Kmem::kcode_start()
{ return Mem_layout::Kernel_start_frame; }

IMPLEMENT inline Address Kmem::kcode_end()
{ return Mem_layout::Kernel_end_frame; }

PUBLIC static FIASCO_INIT
void
Kmem::init_mmu(Cpu const &boot_cpu)
{
  Kmem_alloc *const alloc = Kmem_alloc::allocator();

  kdir = (Pdir*)alloc->alloc(Config::PAGE_SHIFT);
  memset (kdir, 0, Config::PAGE_SIZE);

  Pt_entry::have_superpages(boot_cpu.superpages());
  if (boot_cpu.features() & FEAT_PGE)
    Pt_entry::enable_global();

  // set up the kernel mapping for physical memory.  mark all pages as
  // referenced and modified (so when touching the respective pages
  // later, we save the CPU overhead of marking the pd/pt entries like
  // this)

  // we also set up a one-to-one virt-to-phys mapping for two reasons:
  // (1) so that we switch to the new page table early and re-use the
  // segment descriptors set up by bootstrap.c.  (we'll set up our own
  // descriptors later.)  (2) a one-to-one phys-to-virt mapping in the
  // kernel's page directory sometimes comes in handy
  kdir->map(0, Virt_addr(Mem_layout::Physmem), Virt_size(Mem_layout::pmem_size),
            Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::global(),
            Pt_entry::super_level(), false, pdir_alloc(alloc));

  // now switch to our new page table
  Emulation::set_pdir_addr (Mem_layout::pmem_to_phys (kdir));

  // map the cpu_page we allocated earlier just before io_bitmap
  assert((Mem_layout::Io_bitmap & ~Config::SUPERPAGE_MASK) == 0);

  if (boot_cpu.superpages()
      && Config::SUPERPAGE_SIZE - (pmem_cpu_page & ~Config::SUPERPAGE_MASK) < 0x10000)
    {
      // can map as 4MB page because the cpu_page will land within a
      // 16-bit range from io_bitmap
      kdir->walk(Virt_addr(Mem_layout::Io_bitmap - Config::SUPERPAGE_SIZE),
                 Pdir::Super_level, false, pdir_alloc(alloc)).
        set_page(pmem_cpu_page & Config::SUPERPAGE_MASK,
                 Pt_entry::Pse_bit
                 | Pt_entry::Writable | Pt_entry::Referenced
                 | Pt_entry::Dirty | Pt_entry::global());

      cpu_page_vm = (pmem_cpu_page & ~Config::SUPERPAGE_MASK)
                    + (Mem_layout::Io_bitmap - Config::SUPERPAGE_SIZE);
    }
  else
    {
      auto pt = kdir->walk(Virt_addr(Mem_layout::Io_bitmap - Config::PAGE_SIZE),
                           Pdir::Depth, false, pdir_alloc(alloc));

      pt.set_page(pmem_cpu_page,
                  Pt_entry::Writable
                  | Pt_entry::Referenced | Pt_entry::Dirty
                  | Pt_entry::global());

      cpu_page_vm = Mem_layout::Io_bitmap - Config::PAGE_SIZE;
    }

  if (mmap ((void *) cpu_page_vm, Config::PAGE_SIZE, PROT_READ 
            | PROT_WRITE, MAP_SHARED | MAP_FIXED, Boot_info::fd(), pmem_cpu_page)
      == MAP_FAILED)
    printf ("CPU page mapping failed: %s\n", strerror (errno));

  Cpu::init_tss (alloc_tss(sizeof(Tss)));

}

