//---------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "config.h"
#include "panic.h"
#include "warn.h"
#include "mem.h"
#include "mem_space.h"
#include "kmem_alloc.h"

#include <cstdio>

IMPLEMENT
void Vmem_alloc::init()
{
  NOT_IMPL_PANIC;
}

IMPLEMENT
void *Vmem_alloc::page_alloc(void *address, Zero_fill zf, unsigned mode)
{
  void *vpage = Kmem_alloc::allocator()->alloc(Config::page_order());

  if (EXPECT_FALSE(!vpage))
    return 0;

  Address page = Kmem::kdir->virt_to_phys((Address)vpage);
  if (0)
    printf("  allocated page (virt=%p, phys=%08lx)\n", vpage, page);

  auto pte = Kmem::kdir->walk(Virt_addr(address),
                              Pdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root.unwrap()));

  Page::Rights r = Page::Rights::RWX();
  if (mode & User)
    r |= Page::Rights::U();

  pte.create_page(Phys_mem_addr(page), Page::Attr::kern_global(r));
  pte.write_back_if(true);
  //Mem_unit::dtlb_flush(address);

  if (zf == ZERO_FILL)
    Mem::memset_mwords((unsigned long *)address, 0, Config::PAGE_SIZE >> 2);
  return address;
}
