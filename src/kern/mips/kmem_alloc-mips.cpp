/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */

IMPLEMENTATION [mips]:

#include "mem_unit.h"
#include "ram_quota.h"
#include "mem_region.h"
#include "kmem.h"
#include "koptions.h"
#include "paging_bits.h"

#include <cstdio>


enum { Freemap_size = Kmem_alloc::Alloc::free_map_bytes(0, (512UL << 20) - 1) };
static unsigned long _freemap[Freemap_size / sizeof (unsigned long)];

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Mem_region_map<64> map;
  Mem_region f;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  printf("Available physical memory: %#lx\n", available_size);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)\n",
          available_size);

  unsigned long alloc_size = Koptions::o()->kmemsize << 10;
  if (!alloc_size)
    alloc_size = available_size / 100 * Config::Kmem_per_cent;

  if (alloc_size > (Config::Kmem_max_mb << 20))
    alloc_size = Config::Kmem_max_mb << 20;

  alloc_size = Pg::round(alloc_size);

  // limit to the lower 512MB region to make sure it is mapped in KSEG0
  unsigned long max_addr = 512UL << 20;

  a->init(Mem_layout::phys_to_pmem(0)); //Mem_layout::phys_to_pmem(f.start));
  a->setup_free_map(_freemap, Freemap_size);

  _orig_free = alloc_size;
  for (unsigned i = 0; alloc_size && (i < map.length()); ++i) {
    f = map[i];

    if (f.start >= max_addr)
      continue;

    unsigned long sz = f.size() > alloc_size ? alloc_size : f.size();

    if ((f.start + sz) >= max_addr)
      sz = max_addr - f.start;

    if (sz < Config::PAGE_SIZE)
      continue;

    Kip::k()->add_mem_region(Mem_desc(f.start, f.start + sz - 1, Mem_desc::Reserved));
    printf("Add KMEM memory @ %#lx, size %#lx\n", Mem_layout::phys_to_pmem(f.start), sz);
    a->add_mem((void *)Mem_layout::phys_to_pmem(f.start), sz);

    alloc_size -= sz;
  }

  if (alloc_size)
    panic("Kmem_alloc: cannot allocate sufficient kernel memory");
}

//----------------------------------------------------------------------------
IMPLEMENTATION [mips && debug]:

#include <cstdio>

#include "kip_init.h"
#include "panic.h"

PUBLIC
void Kmem_alloc::debug_dump()
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %ldKB out of %ldKB of Kmem\n",
         (_orig_free - free + 1023) / 1024,
         (_orig_free + 1023) / 1024);
}
