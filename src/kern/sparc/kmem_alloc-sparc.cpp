//----------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "kmem.h"
#include "mem_region.h"
#include "psr.h"

#include <cstdio>

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Mword alloc_size = Config::Kmem_size;
  a->init(Mem_layout::Map_base);

  /* First, collect non-used physical memory chunks
   * into a list. */
  Mem_region_map<64> map;
  unsigned long avail_size = create_free_map(Kip::k(), &map);
  printf("Available phys mem: %08lx\n", avail_size);

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
        f.start += (f.size() - alloc_size);

      printf("  [%08lx - %08lx %4ld kB]\n", f.start, f.end, f.size() >> 10);
      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Reserved));
      printf("    -> %08lx - %08lx\n",
	     Mem_layout::phys_to_pmem(f.start),
	     Mem_layout::phys_to_pmem(f.end));
      a->add_mem((void*)Mem_layout::phys_to_pmem(f.start), f.size());
      alloc_size -= f.size();
    }
}

PUBLIC inline NEEDS["kmem.h", "psr.h", <cstdio>]
Address
Kmem_alloc::to_phys(void *v) const
{
  Address p = Kmem::kdir->virt_to_phys((Address)v);
  printf("Kmem_alloc::to_phys: v=%p p=%lx psr=%lx\n", v, p, Psr::read());
  return p;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [sparc && debug]:

PUBLIC
void Kmem_alloc::debug_dump()
{
  a->dump();
}
