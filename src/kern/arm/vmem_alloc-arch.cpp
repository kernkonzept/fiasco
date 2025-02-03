//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu]:

#include "mem.h"
#include "paging.h"
#include "mem_space.h"
#include "kmem_alloc.h"
#include "config.h"
#include "panic.h"
#include "mem_unit.h"
#include "ram_quota.h"
#include "static_init.h"

#include <cstdio>
#include <cassert>
#include <cstring>


IMPLEMENT
void *Vmem_alloc::page_alloc(void *address, Zero_fill zf, unsigned mode)
{
  void *vpage = Kmem_alloc::allocator()->alloc(Config::page_order());

  if (EXPECT_FALSE(!vpage))
    return nullptr;

  Address page = Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(vpage));
  if constexpr (0) // Intentionally disabled, only used for diagnostics
    printf("  allocated page (virt=%p, phys=%08lx\n", vpage, page);
  Mem_unit::inv_dcache(vpage, offset_cast<void *>(vpage, Config::PAGE_SIZE));

  // insert page into master page table
  auto pte = Kmem::kdir->walk(Virt_addr(address),
                              Kpdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root.unwrap()));

  Page::Rights r = Page::Rights::RWX();
  if (mode & User)
    r |= Page::Rights::U();

  pte.set_page(Phys_mem_addr(page), Page::Attr::kern_global(r));
  pte.write_back_if(true);
  Mem_unit::tlb_flush_kernel(reinterpret_cast<Address>(address));

  if (zf == ZERO_FILL)
    Mem::memset_mwords(address, 0, Config::PAGE_SIZE / sizeof(Mword));

  return address;
}

