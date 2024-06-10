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

//---------------------------------------------------------------------------
IMPLEMENTATION[arm && 64bit && kernel_nx && mmu]:

template <typename PDIR>
static inline void
Kmem::drop_rights(PDIR *kd, Address vstart, Address vend, Page::Rights del)
{
  vstart = Super_pg::trunc(vstart);
  vend = Super_pg::round(vend);
  unsigned long size = vend - vstart;

  for (unsigned long i = 0; i < size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = kd->walk(Virt_addr(vstart + i), PDIR::Super_level);
      assert(pte.is_valid());
      assert(pte.is_leaf());
      assert(pte.page_order() == Config::SUPERPAGE_SHIFT);
      assert(pte.attribs().rights & del);
      pte.del_rights(del);
      pte.write_back_if(true);
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && kernel_nx && !cpu_virt && mmu]:

#include "kmem_space.h"
#include "mem_layout.h"

IMPLEMENT_OVERRIDE
static
void
Kmem::kernel_remap()
{
  extern Pdir kernel_l0_dir;
  Pdir *udir = &kernel_l0_dir;

  extern char *_kernel_image_start;
  extern char *_rx_start;
  extern char *_rx_end;
  extern char *_initcall_end;

  Address kernel_start = reinterpret_cast<Address>(&_kernel_image_start);
  Address rx_start = reinterpret_cast<Address>(&_rx_start);
  Address rx_end = reinterpret_cast<Address>(&_rx_end);
  Address kernel_end = reinterpret_cast<Address>(&_initcall_end);

  Address kernel_start_phys = kdir->virt_to_phys(kernel_start);
  Address rx_start_phys = kdir->virt_to_phys(rx_start);
  Address rx_end_phys = kdir->virt_to_phys(rx_end);
  Address kernel_end_phys = kdir->virt_to_phys(kernel_end);

  drop_rights(kdir.unwrap(), kernel_start, rx_start, Page::Rights::X());
  drop_rights(kdir.unwrap(), rx_start, rx_end, Page::Rights::W());
  drop_rights(kdir.unwrap(), rx_end, kernel_end, Page::Rights::X());

  drop_rights(udir, kernel_start_phys, rx_start_phys, Page::Rights::X());
  drop_rights(udir, rx_start_phys, rx_end_phys, Page::Rights::W());
  drop_rights(udir, rx_end_phys, kernel_end_phys, Page::Rights::X());

  Mem_unit::tlb_flush_kernel();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && kernel_nx && cpu_virt && mmu]:

#include "kmem_space.h"
#include "mem_layout.h"

IMPLEMENT_OVERRIDE
static
void
Kmem::kernel_remap()
{
  extern char *_kernel_image_start;
  extern char *_rx_start;
  extern char *_rx_end;
  extern char *_initcall_end;

  Address kernel_start = reinterpret_cast<Address>(&_kernel_image_start);
  Address rx_start = reinterpret_cast<Address>(&_rx_start);
  Address rx_end = reinterpret_cast<Address>(&_rx_end);
  Address kernel_end = reinterpret_cast<Address>(&_initcall_end);

  Address kernel_start_phys = kdir->virt_to_phys(kernel_start);
  Address rx_start_phys = kdir->virt_to_phys(rx_start);
  Address rx_end_phys = kdir->virt_to_phys(rx_end);
  Address kernel_end_phys = kdir->virt_to_phys(kernel_end);

  drop_rights(kdir.unwrap(), kernel_start, rx_start, Page::Rights::X());
  drop_rights(kdir.unwrap(), rx_start, rx_end, Page::Rights::W());
  drop_rights(kdir.unwrap(), rx_end, kernel_end, Page::Rights::X());

  if (kernel_start != kernel_start_phys)
    {
      drop_rights(kdir.unwrap(), kernel_start_phys, rx_start_phys, Page::Rights::X());
      drop_rights(kdir.unwrap(), rx_start_phys, rx_end_phys, Page::Rights::W());
      drop_rights(kdir.unwrap(), rx_end_phys, kernel_end_phys, Page::Rights::X());
    }

  Mem_unit::tlb_flush_kernel();
}
