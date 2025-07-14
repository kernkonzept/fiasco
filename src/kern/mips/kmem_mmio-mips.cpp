IMPLEMENTATION [mips]:

#include <cassert>
#include "mem_layout.h"

IMPLEMENT_OVERRIDE
uintptr_t
Kmem_mmio::find_unmapped_extent(size_t, uintptr_t, uintptr_t, size_t, unsigned)
{ return invalid_ptr; }

IMPLEMENT_OVERRIDE
uintptr_t
Kmem_mmio::find_mapped_extent(Address, size_t, uintptr_t, uintptr_t,
                              Page::Attr, size_t, unsigned)
{ return invalid_ptr; }

IMPLEMENT_OVERRIDE
bool
Kmem_mmio::map_extent(Address, uintptr_t, size_t, Page::Attr, unsigned)
{ return false; }

IMPLEMENT_OVERRIDE
void *
Kmem_mmio::map(Address phys, [[maybe_unused]] size_t size, Map_attr)
{
  assert((phys + size <= Mem_layout::Mmio_map_end - Mem_layout::Mmio_map_start)
         && "MMIO outside KSEG1");
  return reinterpret_cast<void *>(phys + Mem_layout::Mmio_map_start);
}

IMPLEMENT_OVERRIDE
void
Kmem_mmio::unmap(void *, size_t)
{}
