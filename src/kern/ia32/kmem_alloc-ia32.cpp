IMPLEMENTATION [ia32,ux,amd64]:

// base_init() puts those Mem_region_map's on the stack which is slightly
// larger than our warning limit, it's init code only, so it's ok
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include <cstdio>

#include "kip.h"
#include "koptions.h"
#include "mem_region.h"
#include "panic.h"
#include "types.h"

PUBLIC static inline
Phys_mem_addr::Value
Kmem_alloc::to_phys(void *v)
{
  return Mem_layout::pmem_to_phys(v);
}

PUBLIC static FIASCO_INIT
bool
Kmem_alloc::base_init()
{
  if (0)
    printf("Kmem_alloc::base_init(): kip=%p\n", Kip::k());
  unsigned long available_size = 0;
  unsigned long requested_size;

  Mem_region_map<64> map;

  available_size = create_free_map(Kip::k(), &map);

  requested_size = Koptions::o()->kmemsize << 10;
  if (!requested_size)
    {
      requested_size = available_size / 100 * Config::kernel_mem_per_cent;
      if (requested_size > Config::kernel_mem_max)
	requested_size = Config::kernel_mem_max;
    }

  if (requested_size > Mem_layout::Physmem_max_size)
    requested_size = Mem_layout::Physmem_max_size; // maximum mappable memory

  requested_size =    (requested_size + Config::PAGE_SIZE - 1)
                   & ~(Config::PAGE_SIZE - 1);

  if (0)
    {
      printf("Kmem_alloc: available_memory=%lu KB requested_size=%lu\n",
             available_size / 1024, requested_size / 1024);

      printf("Kmem_alloc:: available blocks:\n");
      for (unsigned i = 0; i < map.length(); ++i)
        printf("  %2u [%014lx; %014lx)\n", i, map[i].start, map[i].end + 1);
    }

  unsigned long base = 0;
  unsigned long sp_base = 0;
  unsigned long end = map[map.length() - 1].end;
  unsigned last = map.length();
  unsigned i;
  unsigned long size = requested_size;

  for (i = last; i > 0 && size > 0; --i)
    {
      if (map[i - 1].size() >= size)
	{ // next block is sufficient
	  base = map[i - 1].end - size + 1;
	  sp_base = base & ~(Config::SUPERPAGE_SIZE - 1);
	  if ((end - sp_base + 1) > (Mem_layout::Physmem_max_size))
	    {
	      if (last == i)
		{ // already a single block, try to align
		  if (sp_base >= map[i - 1].start)
		    {
		      base = sp_base;
		      end  = sp_base + size - 1;
                      size = 0;
		    }
		  continue;
		}
	      else if (last > 1)
		{ // too much virtual memory, try other blocks
                  // free last block
		  size += map[last - 1].size();
		  end = map[last - 2].end;
		  --last;
                  ++i; // try same block again
                  continue;
		}
	    }
	  else
	    size = 0;
	}
      else
	size -= map[i - 1].size();
    }

  if (size)
    return false;

  if (0)
    {
      printf("Kmem_alloc: kernel memory from %014lx to %014lx\n", base, end + 1);
      printf("Kmem_alloc: blocks %u-%u\n", i, last - 1);
    }

  Kip::k()->add_mem_region(Mem_desc(base, end <= map[i].end ? end : map[i].end,
                                    Mem_desc::Kernel_tmp));
  ++i;
  for (; i < last; ++i)
    Kip::k()->add_mem_region(Mem_desc(map[i].start, map[i].end,
                                      Mem_desc::Kernel_tmp));

  Mem_layout::kphys_base(sp_base);
  Mem_layout::pmem_size =    (end + 1 - sp_base + Config::SUPERPAGE_SIZE - 1)
                          & ~(Config::SUPERPAGE_SIZE - 1);
  assert(Mem_layout::pmem_size <= Config::kernel_mem_max);

  return true;
}

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  if (0)
    printf("Kmem_alloc::Kmem_alloc()\n");

  unsigned long min_addr = ~0UL;
  unsigned long max_addr = 0;

  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual())
        continue;

      if (md.type() != Mem_desc::Kernel_tmp)
        continue;

      if (min_addr > md.start())
        min_addr = md.start();

      if (max_addr < md.end())
        max_addr = md.end();
    }

  if (min_addr >= max_addr)
    panic("cannot allocate kernel memory, invalid reserved areas\n");

  unsigned long freemap_size = Alloc::free_map_bytes(min_addr, max_addr);

  if (0)
    printf("Kmem_alloc: freemap needs %lu bytes\n", freemap_size);

  // mem descriptor where we allocate the freemap
  Mem_desc *bmmd = nullptr;

  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual())
        continue;

      if (md.type() != Mem_desc::Kernel_tmp)
        continue;

      unsigned long size = md.end() - md.start() + 1;
      if (size < freemap_size)
        continue;

      bmmd = &md;
      break;
    }

  if (!bmmd)
    panic("Kmem_alloc: could not allocate buddy freemap\n");

  unsigned long min_addr_kern = Mem_layout::phys_to_pmem(min_addr);
  unsigned long bm_kern = Mem_layout::phys_to_pmem(bmmd->start());

  if (bm_kern == min_addr_kern)
    min_addr_kern += freemap_size;

  if (0)
    printf("Kmem_alloc: allocator base = %014lx\n",
           Kmem_alloc::Alloc::calc_base_addr(min_addr_kern));

  a->init(min_addr_kern);
  a->setup_free_map(reinterpret_cast<unsigned long *>(bm_kern), freemap_size);

  unsigned long size = bmmd->end() - bmmd->start() + 1;
  if (size > freemap_size)
    {
      unsigned long sz = size - freemap_size;
      if (0)
        printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
               bm_kern + freemap_size, min_addr_kern, sz);
      a->add_mem((void*)(bm_kern + freemap_size), sz);
      _orig_free += sz;
    }

  bmmd->type(Mem_desc::Reserved);

  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual())
        continue;

      if (md.type() != Mem_desc::Kernel_tmp)
        continue;

      unsigned long s = md.start(), e = md.end();
      unsigned long s_v = Mem_layout::phys_to_pmem(s);

      if (0)
        printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
               s_v, s, e - s + 1);
      a->add_mem((void *)s_v, e - s + 1);
      md.type(Mem_desc::Reserved);
      _orig_free += e - s + 1;
    }
  if (0)
    printf("Kmem_alloc: construction done\n");
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [{ia32,ux,amd64}-debug]:

#include "div32.h"

PUBLIC
void
Kmem_alloc::debug_dump()
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %lu%%, %luKB out of %luKB of Kmem\n",
         (unsigned long)div32(100ULL * (orig_free() - free), orig_free()),
	 (orig_free() - free + 1023) / 1024,
	 (orig_free()        + 1023) / 1024);
}

PRIVATE inline
unsigned long
Kmem_alloc::orig_free()
{
  return _orig_free;
}
