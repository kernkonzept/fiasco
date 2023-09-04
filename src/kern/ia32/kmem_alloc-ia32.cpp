IMPLEMENTATION [ia32,ux,amd64]:

// base_init() puts those Mem_region_map's on the stack which is slightly
// larger than our warning limit, it's init code only, so it's ok
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include <cstdio>

#include "kip.h"
#include "koptions.h"
#include "mem_region.h"
#include "minmax.h"
#include "panic.h"
#include "types.h"
#include "paging_bits.h"

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
    printf("Kmem_alloc::base_init(): kip=%p\n", Kip::k());

  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);
  unsigned long requested_size = Koptions::o()->kmemsize << 10;
  if (!requested_size)
    {
      requested_size = available_size / 100 * Config::kernel_mem_per_cent;
      if (requested_size > Config::kernel_mem_max)
        requested_size = Config::kernel_mem_max;
    }

  if (requested_size > Mem_layout::Physmem_max_size)
    requested_size = Mem_layout::Physmem_max_size; // maximum mappable memory

  requested_size = Pg::round(requested_size);

  if (0)
    {
      printf("Kmem_alloc: available_memory=%lu KB requested_size=%lu KB\n",
             available_size / 1024, requested_size / 1024);

      printf("Kmem_alloc: available blocks:\n");
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
        {
          // next block is sufficient
          base = map[i - 1].end - size + 1;
          sp_base = Super_pg::trunc(base);
          if (end - sp_base + 1 > Mem_layout::Physmem_max_size)
            {
              // Too much virtual memory. This happens typically if there are
              // multiple blocks from `map` used and there are larger gaps
              // between the blocks.
              if (last == i)
                {
                  // already a single block: try to align
                  if (sp_base >= map[i - 1].start)
                    {
                      base = sp_base;
                      end  = sp_base + size - 1;
                      size = 0;
                    }
                }
              else if (last > 1)
                {
                  // otherwise: try other blocks + free last block
                  size += map[last - 1].size();
                  end = map[last - 2].end;
                  --last;
                  ++i; // try same block again
                }
            }
          else
            size = 0; // done
        }
      else
        size -= map[i - 1].size(); // not done yet
    }

  if (size)
    return false;

  if (0)
    {
      printf("Kmem_alloc: kernel memory from %014lx to %014lx\n", base, end + 1);
      printf("Kmem_alloc: blocks %u-%u\n", i, last - 1);
    }

  Kip::k()->add_mem_region(Mem_desc(base, min(end, map[i].end),
                                    Mem_desc::Kernel_tmp));
  ++i;
  for (; i < last; ++i)
    Kip::k()->add_mem_region(Mem_desc(map[i].start, map[i].end,
                                      Mem_desc::Kernel_tmp));

  Mem_layout::kphys_base(sp_base);
  Mem_layout::pmem_size = Super_pg::round(end + 1 - sp_base);
  assert(Mem_layout::pmem_size <= Config::kernel_mem_max);

  return true;
}

/**
 * Allocate memory for the buddy allocator freemap and for the kernel memory
 * allocator from KIP memory regions marked as `Kernel_tmp`.
 *
 * Walk through all `Kernel_tmp` KIP memory regions and look for a suitable
 * region for the buddy free bitmap. Add the remaining part of this region and
 * all other `Kernel_tmp` regions as kernel memory. Finally change the type of
 * the regions to `Reserved`.
 *
 * The amount of kernel memory is decreased by the size of the free bitmap.
 */
IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  if (0)
    printf("Kmem_alloc::Kmem_alloc()\n");

  unsigned long min_addr = ~0UL;
  unsigned long max_addr = 0;

  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual() || md.type() != Mem_desc::Kernel_tmp)
        continue;

      min_addr = min(min_addr, md.start());
      max_addr = max(max_addr, md.end());
    }

  if (min_addr >= max_addr)
    panic("Cannot allocate kernel memory, invalid reserved areas\n");

  unsigned long freemap_size = Alloc::free_map_bytes(min_addr, max_addr);

  if (0)
    printf("Kmem_alloc: buddy freemap needs %lu bytes\n", freemap_size);

  // find suitable memory descriptor to allocate the buddy freemap (bm)
  Mem_desc *bmmd = nullptr;
  unsigned long bmmd_size = 0;
  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual() || md.type() != Mem_desc::Kernel_tmp)
        continue;

      bmmd_size = md.end() - md.start() + 1;
      if (bmmd_size >= freemap_size)
        {
          bmmd = &md;
          break;
        }
    }

  if (!bmmd)
    panic("Could not allocate buddy freemap\n");

  unsigned long min_addr_kern = Mem_layout::phys_to_pmem(min_addr);
  unsigned long bm_kern = Mem_layout::phys_to_pmem(bmmd->start());

  // Strictly speaking this is not necessary but it also doesn't make sense to
  // initialize the lower boundary of the kernel memory at the buddy freemap.
  if (bm_kern == min_addr_kern)
    min_addr_kern += freemap_size;

  if (0)
    printf("Kmem_alloc: allocator base = %014lx\n",
           Kmem_alloc::Alloc::calc_base_addr(min_addr_kern));

  a->init(min_addr_kern);
  a->setup_free_map(reinterpret_cast<unsigned long *>(bm_kern), freemap_size);

  if (bmmd_size > freemap_size)
    {
      unsigned long add_sz = bmmd_size - freemap_size;
      if (0)
        printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
               bm_kern + freemap_size, min_addr_kern, add_sz);
      // remaining of bmmd (after buddy freemap) for kernel memory
      a->add_mem((void*)(bm_kern + freemap_size), add_sz);
      _orig_free += add_sz;
    }

  bmmd->type(Mem_desc::Reserved);

  for (auto &md: Kip::k()->mem_descs_a())
    {
      if (!md.valid() || md.is_virtual() || md.type() != Mem_desc::Kernel_tmp)
        continue;

      unsigned long md_start = md.start(), md_size = md.end() - md.start() + 1;
      unsigned long md_kern = Mem_layout::phys_to_pmem(md_start);

      if (0)
        printf("  Kmem_alloc: block %014lx(%014lx) size=%lx\n",
               md_kern, md_start, md_size);
      a->add_mem((void *)md_kern, md_size);
      md.type(Mem_desc::Reserved);
      _orig_free += md_size;
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
         (orig_free() - free + 1023) / 1024, (orig_free() + 1023) / 1024);
}

PRIVATE inline
unsigned long
Kmem_alloc::orig_free()
{
  return _orig_free;
}
