IMPLEMENTATION [riscv]:

#include "boot_infos.h"
#include "config.h"
#include "kmem.h"

#include <cstdio>

PUBLIC inline NEEDS["mem_layout.h"]
Address
Kmem_alloc::to_phys(void *v) const
{
  return Mem_layout::pmem_to_phys(reinterpret_cast<Address>(v));
}

static unsigned long _freemap[
  Kmem_alloc::Alloc::free_map_bytes(Mem_layout::Pmem_start,
                                    Mem_layout::Pmem_end - 1)
  / sizeof(unsigned long)];

IMPLEMENT
Kmem_alloc::Kmem_alloc()
{
  Mword alloc_size = Config::KMEM_SIZE;
  Mem_region_map<64> map;
  unsigned long available_size = create_free_map(Kip::k(), &map);

  // sanity check whether the KIP has been filled out, number is arbitrary
  if (available_size < (1 << 18))
    panic("Kmem_alloc: No kernel memory available (%ld)",
          available_size);

  a->init(Mem_layout::Pmem_start);
  a->setup_free_map(_freemap, Kmem_alloc::Alloc::free_map_bytes(
    Mem_layout::Pmem_start, Mem_layout::Pmem_end - 1));

  for (int i = map.length() - 1; i >= 0 && alloc_size > 0; --i)
    {
      Mem_region f = map[i];

      // Align region start address to superpage size
      Address region_start = Mem_layout::round_superpage(f.start);
      Mword region_size = f.size() - (region_start - f.start);

      // Only contiguous mapping is supported for now
      if (region_size < alloc_size)
        continue;

      Address region_end = region_start + alloc_size;

      // Reserve in kernel info page
      Kip::k()->add_mem_region(Mem_desc(region_start, region_end,
                                        Mem_desc::Reserved));

      // Map into virtual address space
      if (!Kmem::boot_map_pmem(region_start, alloc_size))
        panic("Kmem_alloc: cannot map physical memory 0x%lx", region_start);

      // Hand over to allocator
      a->add_mem(
        reinterpret_cast<void *>(Mem_layout::phys_to_pmem(region_start)),
        alloc_size);

      alloc_size = 0;
      break;
    }

  if (alloc_size)
    panic("Kmem_alloc: cannot allocate sufficient kernel memory");
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && debug]:

PUBLIC
void
Kmem_alloc::debug_dump() const
{
  a->dump();

  unsigned long free = a->avail();
  printf("Used %ldKB out of %dKB of Kmem\n",
   (Config::KMEM_SIZE - free + 1023) / 1024,
   (Config::KMEM_SIZE        + 1023) / 1024);
}
