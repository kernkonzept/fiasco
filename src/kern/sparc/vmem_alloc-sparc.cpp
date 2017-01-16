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
  void *vpage = Kmem_alloc::allocator()->alloc(Config::PAGE_SHIFT);

  if (EXPECT_FALSE(!vpage))
    return 0;

  Address page = Kmem::kdir->virt_to_phys((Address)vpage);
  if (0)
    printf("  allocated page (virt=%p, phys=%08lx)\n", vpage, page);

  auto pte = Kmem::kdir->walk(Virt_addr(address),
                              Pdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root));

  Page::Rights r = Page::Rights::RWX();
  if (mode & User)
    r |= Page::Rights::U();

  pte.create_page(Phys_mem_addr(page),
                  Page::Attr(r, Page::Type::Normal(), Page::Kern::Global()));
  pte.write_back_if(true, 0 /*Mem_unit::Asid_kernel*/);
  //Mem_unit::dtlb_flush(address);

  if (zf == ZERO_FILL)
    Mem::memset_mwords((unsigned long *)address, 0, Config::PAGE_SIZE >> 2);
  return address;
}

IMPLEMENT
void *Vmem_alloc::page_unmap(void * /*page*/)
{
  NOT_IMPL_PANIC;
  return NULL;
}
