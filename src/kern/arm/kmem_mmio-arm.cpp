IMPLEMENTATION [arm && mpu]:

#include "mem.h"
#include "kmem.h"

IMPLEMENT_DEFAULT
void *
Kmem_mmio::map(Address phys, size_t size, bool cache = false, bool, bool, bool)
{
  // Arm MPU regions must be aligned to 64 bytes
  Address start = phys & ~63UL;
  Address end = (phys + size - 1U) | 63U;

  // Attention: this only works before the first user space task is created.
  // Otherwise the mpu regions will collide!
  auto diff = Kmem::kdir->add(
    start, end,
    Mpu_region_attr::make_attr(L4_fpage::Rights::RW(),
                               cache ? L4_snd_item::Memory_type::Normal()
                                     : L4_snd_item::Memory_type::Uncached()));
  assert(diff);
  Mpu::sync(*Kmem::kdir, diff.value());
  Mem::isb();

  return reinterpret_cast<void *>(phys);
}

IMPLEMENT_DEFAULT
void
Kmem_mmio::unmap(void *ptr, size_t size)
{
  Mem::dsb();

  // Arm MPU regions must be aligned to 64 bytes
  Address start = reinterpret_cast<Address>(ptr) & ~63UL;
  Address end = (reinterpret_cast<Address>(ptr) + size - 1U) | 63U;

  auto diff = Kmem::kdir->del(start, end, nullptr);
  if (diff)
    Mpu::sync(*Kmem::kdir, diff.value());

  Mem::isb();
}
