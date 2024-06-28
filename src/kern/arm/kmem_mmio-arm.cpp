IMPLEMENTATION [arm && mpu]:

#include "mem.h"
#include "kmem.h"

PUBLIC static
Address
Kmem_mmio::remap(Address phys, Address size, bool cache = false)
{
  // Arm MPU regions must be aligned to 64 bytes
  Address start = phys & ~63UL;
  Address end = (phys + size - 1U) | 63U;

  // Attention: this only works before the first user space task is created.
  // Otherwise the mpu regions will collide!
  auto diff = Kmem::kdir->add(
    start, end,
    Mpu_region_attr::make_attr(L4_fpage::Rights::RW(),
                               cache ? L4_msg_item::Memory_type::Normal()
                                     : L4_msg_item::Memory_type::Uncached()));
  assert(diff);
  Mpu::sync(*Kmem::kdir, diff.value());
  Mem::isb();

  return phys;
}

PUBLIC static
void
Kmem_mmio::unmap_compat(Address virt, Address size)
{
  Mem::dsb();

  // Arm MPU regions must be aligned to 64 bytes
  Address start = virt & ~63UL;
  Address end = (virt + size - 1U) | 63U;

  auto diff = Kmem::kdir->del(start, end, nullptr);
  if (diff)
    Mpu::sync(*Kmem::kdir, diff.value());

  Mem::isb();
}
