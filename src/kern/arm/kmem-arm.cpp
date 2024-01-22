IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
bool
Kmem::is_kmem_page_fault(Mword pfa, Mword)
{
  return in_kernel(pfa);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "paging.h"

IMPLEMENT inline NEEDS["paging.h"]
bool
Kmem::is_kmem_page_fault(Mword, Mword ec)
{
  return !PF::is_usermode_error(ec);
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

#include "kmem_space.h"
#include "mem.h"

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size)
{
  // Arm MPU regions must be aligned to 64 bytes
  Address start = phys & ~63UL;
  Address end = (phys + size - 1U) | 63U;

  // Attention: this only works before the first user space task is created.
  // Otherwise the mpu regions will collide!
  auto diff = kdir->add(
    start, end,
    Mpu_region_attr::make_attr(L4_fpage::Rights::RW(),
                               L4_msg_item::Memory_type::Uncached()));
  assert(diff);
  Mpu::sync(*kdir, diff.value());
  Mem::isb();

  return phys;
}

PUBLIC static
void
Kmem::mmio_unmap(Address virt, Address size)
{
  Mem::dsb();

  // Arm MPU regions must be aligned to 64 bytes
  Address start = virt & ~63UL;
  Address end = (virt + size - 1U) | 63U;

  auto diff = kdir->del(start, end, nullptr);
  if (diff)
    Mpu::sync(*kdir, diff.value());
  Mem::isb();
}
