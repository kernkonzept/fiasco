//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

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
  void *vpage = Kmem_alloc::allocator()->alloc(Config::PAGE_SHIFT);

  if (EXPECT_FALSE(!vpage))
    return 0;

  Address page = Kmem::kdir->virt_to_phys((Address)vpage);
  if (0)
    printf("  allocated page (virt=%p, phys=%08lx\n", vpage, page);
  Mem_unit::inv_dcache(vpage, ((char*)vpage) + Config::PAGE_SIZE);

  // insert page into master page table
  auto pte = Kmem::kdir->walk(Virt_addr(address),
                              Kpdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root));

  Page::Rights r = Page::Rights::RWX();
  if (mode & User)
    r |= Page::Rights::U();

  pte.set_page(pte.make_page(Phys_mem_addr(page),
                             Page::Attr(r, Page::Type::Normal(),
                                        Page::Kern::Global())));
  pte.write_back_if(true, Mem_unit::Asid_kernel);
  Mem_unit::dtlb_flush(address);

  if (zf == ZERO_FILL)
    Mem::memset_mwords((unsigned long *)address, 0, Config::PAGE_SIZE / sizeof(Mword));

  return address;
}

