IMPLEMENTATION [riscv]:

#include "boot_infos.h"
#include "config.h"
#include "paging_bits.h"

#include <cstdio>

PUBLIC static
void
Kmem_alloc::base_init(bool (*map_pmem)(Address phys, Mword size))
{
  Free_regions_map map;
  unsigned long available_size = create_free_map(Kip::k(), &map,
                                                 Config::SUPERPAGE_SIZE);

  unsigned long alloc_size = determine_kmem_alloc_size(available_size,
                                                       Config::SUPERPAGE_SIZE);

  // Walk through all KIP memory regions of conventional memory minus the
  // reserved memory and find one suitable for the kernel memory.
  //
  // The kernel memory regions are added to the KIP as `Kernel_tmp`. Later, in
  // setup_kmem_from_kip_md_tmp(), these regions are added as kernel memory
  // (a()->add_mem()) and marked as "Reserved".
  unsigned long min_virt = ~0UL, max_virt = 0UL;
  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];

      // Align region start address to superpage size
      Address region_start = Super_pg::round(f.start);
      Mword region_size = f.size() - (region_start - f.start);

      // Only contiguous mapping is supported for now
      if (region_size < alloc_size)
        continue;

      Address region_end = region_start + alloc_size;

      // Reserve in kernel info page
      Kip::k()->add_mem_region(Mem_desc(region_start, region_end - 1,
                                        Mem_desc::Kernel_tmp));

      // Map into virtual address space
      if (!map_pmem(region_start, alloc_size))
        panic("Kmem_alloc: cannot map physical memory 0x%lx", region_start);

      min_virt = Mem_layout::Pmem_start;
      max_virt = Mem_layout::Pmem_start + alloc_size;
      alloc_size = 0;
      break;
    }

  if (alloc_size)
    panic("Kmem_alloc: cannot allocate sufficient kernel memory");

  unsigned long freemap_size = Alloc::free_map_bytes(min_virt, max_virt);
  Address min_addr_kern = min_virt;

  setup_kmem_from_kip_md_tmp(freemap_size, min_addr_kern);
}

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{}
