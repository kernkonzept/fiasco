//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "mem.h"
#include "paging.h"
#include "mem_space.h"
#include "kmem_alloc.h"
#include "config.h"
#include "mem_unit.h"
#include "ram_quota.h"

IMPLEMENT
void *
Vmem_alloc::page_alloc(void *address, Zero_fill zf, unsigned mode)
{
  void *vpage = Kmem_alloc::q_allocator(
    Ram_quota::root.unwrap()).alloc(Config::page_size());

  if (EXPECT_FALSE(!vpage))
    return 0;

  Address page = Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(vpage));

  // insert page into master page table
  auto pte = Kmem::kdir->walk(Virt_addr(address),
                              Kpdir::Depth, false,
                              Kmem_alloc::q_allocator(Ram_quota::root.unwrap()));

  Page::Rights r = Page::Rights::RWX();
  if (mode & User)
    r |= Page::Rights::U();

  pte.set_page(Phys_mem_addr(page), Page::Attr::kern_global(r));

  // Full tlb flush as global mappings have been changed.
  Mem_unit::tlb_flush();

  {
    Cpu::User_mem_access uma(mode & User);

    if (zf == ZERO_FILL)
      Mem::memset_mwords(reinterpret_cast<unsigned long *>(address), 0,
                         Config::PAGE_SIZE / sizeof(Mword));
  }

  return address;
}
