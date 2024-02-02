INTERFACE [ux]:

class Kernel_task;

EXTENSION class Kmem
{
protected:
  friend class Kernel_task;
};

IMPLEMENTATION [ux]:

#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>

#include "boot_info.h"
#include "kmem_alloc.h"
#include "emulation.h"
#include "paging_bits.h"


IMPLEMENT inline NEEDS ["regdefs.h"]
bool
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

  kdir = static_cast<Kpdir*>(alloc->alloc(Config::page_order()));
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
  if (!kdir->map(0, Virt_addr(Mem_layout::Physmem),
                 Virt_size(Mem_layout::pmem_size),
                 Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::global(),
                 Pt_entry::super_level(), false, pdir_alloc(alloc)))
    panic("Cannot map initial memory");

  // now switch to our new page table
  Emulation::set_pdir_addr (Mem_layout::pmem_to_phys (kdir));

  static_assert(Super_pg::aligned(Mem_layout::Tss_start));

  if (boot_cpu.superpages())
    {
      Address tss_mem_pm_base = Super_pg::trunc(Kmem_alloc::tss_mem_pm);
      size_t tss_mem_pm_extra = Kmem_alloc::tss_mem_pm - tss_mem_pm_base;
      size_t superpages
        = Super_pg::count(Super_pg::round(Mem_layout::Tss_mem_size
                                          + tss_mem_pm_extra));

      for (size_t i = 0; i < superpages; ++i)
        {
          auto e = kdir->walk(Virt_addr(Mem_layout::Tss_start
                                        + Super_pg::size(i)),
                              Pdir::Super_level, false, pdir_alloc(alloc));

          e.set_page(tss_mem_pm_base + Super_pg::size(i),
                     Pt_entry::Pse_bit | Pt_entry::Writable
                     | Pt_entry::Referenced | Pt_entry::Dirty
                     | Pt_entry::global());
        }

      tss_mem_vm.construct(Mem_layout::Tss_start + tss_mem_pm_extra,
                           Mem_layout::Tss_mem_size);
    }
  else
    {
      size_t pages = Pg::count(Pg::round(Mem_layout::Tss_mem_size));

      for (size_t i = 0; i < pages; ++i)
        {
          auto e = kdir->walk(Virt_addr(Mem_layout::Tss_start + Pg::size(i)),
                              Pdir::Depth, false, pdir_alloc(alloc));

          e.set_page(Kmem_alloc::tss_mem_pm + Pg::size(i),
                     Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::Dirty
                     | Pt_entry::global());
        }

      tss_mem_vm.construct(Mem_layout::Tss_start, Mem_layout::Tss_mem_size);
    }

  if (mmap(tss_mem_vm->ptr(), Config::PAGE_SIZE, PROT_READ
           | PROT_WRITE, MAP_SHARED | MAP_FIXED, Boot_info::fd(),
           Kmem_alloc::tss_mem_pm)
      == MAP_FAILED)
    printf("CPU page mapping failed: %s\n", strerror(errno));

  kdir->walk(Virt_addr(Mem_layout::Service_page), Pdir::Depth,
             false, pdir_alloc(alloc));

  Cpu::init_tss(tss_mem_vm->alloc<Tss>(1, Order(4)));
}
