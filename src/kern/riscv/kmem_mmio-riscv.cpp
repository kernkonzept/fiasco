IMPLEMENTATION [riscv]:

#include "kmem.h"

IMPLEMENT_OVERRIDE
Pte_ptr
Kmem_mmio::boot_kdir_walk(Virt_addr virt, unsigned level)
{
  return Kmem::boot_kdir_walk(virt, level);
}

IMPLEMENT_OVERRIDE
bool
Kmem_mmio::boot_kdir_map(Address phys, Virt_addr virt, Virt_size size,
                         Page::Attr attr, unsigned level)
{
  return Kmem::boot_kdir_map(phys, virt, size, attr, level);
}
