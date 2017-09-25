IMPLEMENTATION [arm]:
// Kmem_alloc::Kmem_alloc() puts those Mem_region_map's on the stack which
// is slightly larger than our warning limit but it's on the boot stack only
// so this it is ok.
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include "mem_unit.h"
#include "kmem_space.h"
#include "ram_quota.h"

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && !hyp]:

PRIVATE //inline
bool
Kmem_alloc::map_pmem(unsigned long phy, unsigned long size)
{
  static unsigned long next_map = Mem_layout::Pmem_start;
  size = Mem_layout::round_superpage(size + (phy & ~Config::SUPERPAGE_MASK));
  phy = Mem_layout::trunc_superpage(phy);

  if (next_map + size > Mem_layout::Pmem_end)
    return false;

  for (unsigned long i = 0; i <size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = Kmem_space::kdir()->walk(Virt_addr(next_map + i), Pdir::Super_level);
      pte.create_page(Phys_mem_addr(phy + i), Page::Attr(Page::Rights::RW()));
      pte.write_back_if(true, Mem_unit::Asid_kernel);
    }
  Mem_layout::add_pmem(phy, next_map, size);
  next_map += size;
  return true;
}

PUBLIC inline NEEDS["kmem_space.h"]
Address
Kmem_alloc::to_phys(void *v) const
{
  return Kmem_space::kdir()->virt_to_phys((Address)v);
}


IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  // The -Wframe-larger-than= warning for this function is known and
  // no problem, because the function runs only on our boot stack.
  Mword alloc_size = Config::KMEM_SIZE;
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)\n",
          available_size);

  Mem_region last = map[map.length() - 1];
  if (last.end - Mem_layout::Sdram_phys_base < Config::kernel_mem_max)
    a->init(Mem_layout::Map_base);
  else
    a->init(Mem_layout::Pmem_start);

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
	f.start += (f.size() - alloc_size);

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Reserved));
      if (0)
	printf("Kmem_alloc: [%08lx; %08lx] sz=%ld\n", f.start, f.end, f.size());
      if (Mem_layout::phys_to_pmem(f.start) == ~0UL)
	if (!map_pmem(f.start, f.size()))
	  panic("Kmem_alloc: cannot map physical memory %p\n", (void*)f.start);

      a->add_mem((void *)Mem_layout::phys_to_pmem(f.start), f.size());
      alloc_size -= f.size();
    }

  if (alloc_size)
    WARNX(Warning, "Kmem_alloc: cannot allocate sufficient kernel memory\n");
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && hyp]:

PUBLIC inline NEEDS["kmem_space.h"]
Address
Kmem_alloc::to_phys(void *v) const
{ return (Address)v; }


IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  // The -Wframe-larger-than= warning for this function is known and
  // no problem, because the function runs only on our boot stack.
  Mword alloc_size = Config::KMEM_SIZE;
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)\n",
          available_size);

  Address base = ~0UL;
  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
	f.start += (f.size() - alloc_size);

      if (f.start < base)
        base = f.start;

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Reserved));

      alloc_size -= f.size();
    }

  base &= ~((Address)Config::SUPERPAGE_SIZE - 1);

  a->init(base);
  alloc_size = Config::KMEM_SIZE;

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];
      if (f.size() > alloc_size)
	f.start += (f.size() - alloc_size);

      a->add_mem((void *)f.start, f.size());
      alloc_size -= f.size();
    }

  if (alloc_size)
    WARNX(Warning, "Kmem_alloc: cannot allocate sufficient kernel memory\n");
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && debug]:

#include <cstdio>

#include "kip_init.h"
#include "panic.h"

PUBLIC
void Kmem_alloc::debug_dump()
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %ldKB out of %dKB of Kmem\n",
	 (Config::KMEM_SIZE - free + 1023)/1024,
	 (Config::KMEM_SIZE        + 1023)/1024);
}
