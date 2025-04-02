IMPLEMENTATION[ia32 || amd64]:

#include <cassert>
#include <cstdio>
#include <cstring>
#include "config.h"
#include "kdb_ke.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "paging.h"
#include "static_init.h"
#include "initcalls.h"
#include "space.h"

IMPLEMENT
void*
Vmem_alloc::page_alloc(void *address, Zero_fill zf, unsigned mode)
{
  void *vpage = nullptr;
  Address page;

  vpage = Kmem_alloc::allocator()->alloc(Config::page_order());

  if (EXPECT_FALSE(!vpage))
    return nullptr;

  // insert page into master page table
  auto e = Kmem::kdir->walk(Virt_addr(address), Pdir::Depth,
                            false, pdir_alloc(Kmem_alloc::allocator()));
  if (EXPECT_FALSE(e.is_valid()))
    {
      kdb_ke("page_alloc: address already mapped");
      goto error;
    }

  if (e.level != Pdir::Depth)
    goto error;

  if (zf == ZERO_FILL)
    memset(vpage, 0, Config::PAGE_SIZE);

  page = Mem_layout::pmem_to_phys(reinterpret_cast<Address>(vpage));

  e.set_page(Phys_mem_addr(page),
             Page::Attr(mode & User ? Page::Rights::URW() : Page::Rights::RW(),
             Page::Type::Normal(), Page::Kern::Global(),
             Page::Flags::Touched()));
  return address;

error:
  Kmem_alloc::allocator()->free(Config::page_order(), vpage); // 2^0 = 1 page
  return nullptr;
}
