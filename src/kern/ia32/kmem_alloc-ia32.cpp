INTERFACE [ia32 || amd64]:

#include "types.h"

EXTENSION class Kmem_alloc
{
public:
  static Address tss_mem_pm;
};

IMPLEMENTATION [ia32 || amd64]:

// base_init() puts those Mem_region_map's on the stack which is slightly
// larger than our warning limit, it's init code only, so it's ok
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include <cstdio>

#include "kip.h"
#include "mem_region.h"
#include "minmax.h"
#include "panic.h"
#include "types.h"
#include "paging_bits.h"

Address Kmem_alloc::tss_mem_pm;

/**
 * Walk through all KIP memory regions of conventional memory minus the
 * reserved memory and find one or more regions suitable for the kernel memory.
 *
 * Start at the last region of `map` which is guaranteed to have the highest
 * start address. All regions used for kernel memory including the gaps between
 * them have to fit into Mem_layout::Physmem_max_size because the allocator
 * works on a single memory area with a dedicated start address (a()->init()).
 * It is likely that only a part of the memory region with the lowest start
 * address is used (see `base` below).
 *
 * The kernel memory regions are added to the KIP as `Kernel_tmp`. Later, in
 * Kmem_alloc(), these regions are added as kernel memory (a()->add_mem()) and
 * marked as "Reserved".
 *
 * The kernel memory includes the buddy freemap (see a()->setup_free_map()).
 */
PUBLIC static FIASCO_INIT
bool
Kmem_alloc::base_init()
{
  if (0)
    printf("Kmem_alloc::base_init(): kip=%p\n", static_cast<void *>(Kip::k()));

  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  unsigned long alloc_size = determine_kmem_alloc_size(available_size);

  if (alloc_size > Mem_layout::Physmem_max_size)
    alloc_size = Mem_layout::Physmem_max_size; // maximum mappable memory

  static_assert(Mem_layout::Physmem_max_size % Config::PAGE_SIZE == 0,
                "Physmem_max_size must be page-aligned");

  if (0)
    {
      printf("Kmem_alloc: available_memory=%lu KB alloc_size=%lu KB\n",
             available_size / 1024, alloc_size / 1024);

      printf("Kmem_alloc: available blocks:\n");
      for (unsigned i = 0; i < map.length(); ++i)
        printf("  %2u [%014lx; %014lx)\n", i, map[i].start, map[i].end + 1);
    }

  unsigned long base = 0;
  unsigned long sp_base = 0;
  unsigned long end = map[map.length() - 1].end;
  int last = map.length() - 1;
  int i = last;

  while (i >= 0)
    {
      if (map[i].size() >= alloc_size)
        {
          // next block is sufficient
          base = map[i].end - alloc_size + 1;
          sp_base = Super_pg::trunc(base);
          if (end - sp_base + 1 > Mem_layout::Physmem_max_size)
            {
              // Too much virtual memory. This happens typically if there are
              // multiple blocks from `map` used and there are larger gaps
              // between the blocks.
              if (last == i)
                {
                  // already a single block: try to align
                  if (sp_base >= map[i].start)
                    {
                      base = sp_base;
                      end  = sp_base + alloc_size - 1;
                      alloc_size = 0;
                    }
                }
              else if (last > 0)
                {
                  // otherwise: free last block ...
                  alloc_size += map[last].size();
                  // ... + try next block
                  --last;
                  end = map[last].end;
                  ++i; // try same block again
                }
            }
          else
            alloc_size = 0; // done
        }
      else
        alloc_size -= map[i].size(); // not done yet

      if (!alloc_size)
        break;

      --i;
    }

  if (alloc_size)
    return false;

  if (0)
    {
      printf("Kmem_alloc: kernel memory from %014lx to %014lx\n", base, end + 1);
      printf("Kmem_alloc: blocks %u-%u\n", i, last);
    }

  Kip::k()->add_mem_region(Mem_desc(base, min(end, map[i].end),
                                    Mem_desc::Kernel_tmp));
  ++i;
  for (; i < last; ++i)
    Kip::k()->add_mem_region(Mem_desc(map[i].start, map[i].end,
                                      Mem_desc::Kernel_tmp));

  Mem_layout::kphys_base(sp_base);
  Mem_layout::pmem_size = Super_pg::round(end + 1 - sp_base);

  return true;
}

/**
 * Allocate memory for the buddy allocator freemap, TSSs and for the kernel
 * memory allocator from KIP memory regions marked as `Kernel_tmp`.
 *
 * Walk through all `Kernel_tmp` KIP memory regions and look for a suitable
 * region for the buddy free bitmap and TSSs. Add the remaining parts of the
 * affected regions and all other `Kernel_tmp` regions as kernel memory.
 * Finally, change the type of the regions to `Reserved`.
 *
 * The amount of kernel memory is decreased by the size of the free bitmap and
 * the TSSs.
 */
IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  if (0)
    printf("Kmem_alloc::Kmem_alloc()\n");

  unsigned long min_addr = ~0UL;
  unsigned long max_addr = 0UL;

  for (auto &md: Kip::k()->mem_descs_a())
    if (md.type() == Mem_desc::Kernel_tmp)
      {
        min_addr = min(min_addr, md.start());
        max_addr = max(max_addr, md.end());
      }

  if (min_addr >= max_addr)
    panic("Cannot allocate kernel memory: Invalid reserved areas");

  if (0)
    printf("Kmem_alloc: TSS area needs %zu bytes\n", Mem_layout::Tss_mem_size);

  tss_mem_pm = permanent_alloc(Mem_layout::Tss_mem_size,
                               Order(Config::PAGE_SHIFT));

  unsigned long freemap_size = Alloc::free_map_bytes(min_addr, max_addr);
  Address min_addr_kern = Mem_layout::phys_to_pmem(min_addr);

  setup_kmem_from_kip_md_tmp(freemap_size, min_addr_kern);

  if (0)
    printf("Kmem_alloc: construction done\n");
}
