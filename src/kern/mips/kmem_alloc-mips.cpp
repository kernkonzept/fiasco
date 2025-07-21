/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

IMPLEMENTATION [mips]:

#include "mem_unit.h"
#include "ram_quota.h"
#include "mem_region.h"
#include "paging_bits.h"
#include "panic.h"

#include <cstdio>


IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);
  printf("Available physical memory: %#lx\n", available_size);

  unsigned long alloc_size = determine_kmem_alloc_size(available_size);

  // limit to the lower 512 MiB region to make sure it is mapped in KSEG0
  unsigned long max_addr = 512UL << 20;

  for (unsigned i = 0; alloc_size && i < map.length(); ++i)
    {
      Mem_region f = map[i];

      if (f.start >= max_addr)
        continue;

      if (f.size() > alloc_size)
        f.end -= f.size() - alloc_size;

      if (f.end >= max_addr)
        f.end = max_addr - 1;

      if (f.size() < Config::PAGE_SIZE)
        continue;

      Kip::k()->add_mem_region(Mem_desc(f.start, f.end, Mem_desc::Kernel_tmp));
      printf("Add KMEM memory @ %#lx, size %#lx\n",
             Mem_layout::phys_to_pmem(f.start), f.size());

      alloc_size -= f.size();
    }

  if (alloc_size)
    panic("Kmem_alloc: cannot allocate sufficient kernel memory");

  // We waste a few bytes for the freemap (not exact start/end).
  unsigned long freemap_size = Alloc::free_map_bytes(0UL, max_addr - 1);
  Address min_addr_kern = Mem_layout::phys_to_pmem(0UL);

  setup_kmem_from_kip_md_tmp(freemap_size, min_addr_kern);
}
