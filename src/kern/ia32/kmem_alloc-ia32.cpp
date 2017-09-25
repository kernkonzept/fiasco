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

  if (requested_size > (0 - Mem_layout::Physmem))
    requested_size = 0 - Mem_layout::Physmem; // maximum mappable memory

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
	  if ((end - sp_base + 1) > (0 - Mem_layout::Physmem))
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
  bool initialized = false;

  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (md.is_virtual())
	continue;

      unsigned long s = md.start(), e = md.end();

      // Sweep out stupid descriptors (that have the end before the start)
      if (s >= e)
	{
	  md.type(Mem_desc::Undefined);
	  continue;
	}

      if (md.type() == Mem_desc::Kernel_tmp)
	{
	  unsigned long s_v = Mem_layout::phys_to_pmem(s);
	  if (!initialized)
	    {
	      initialized = true;
	      a->init(s_v & ~(Kmem_alloc::Alloc::Max_size - 1));
	      if (0)
                printf("Kmem_alloc: allocator base = %014lx\n",
                       s_v & ~(Kmem_alloc::Alloc::Max_size - 1));
	    }
	  if (0)
            printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
                   s_v, s, e - s + 1);
	  a->add_mem((void *)s_v, e - s + 1);
	  md.type(Mem_desc::Reserved);
	  _orig_free += e - s + 1;
	}
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
