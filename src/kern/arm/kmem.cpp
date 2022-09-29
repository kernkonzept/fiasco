INTERFACE [arm]:

#include "kip.h"
#include "mem_layout.h"
#include "paging.h"

class Kmem : public Mem_layout
{
public:

  static Mword is_kmem_page_fault(Mword pfa, Mword error);
  static Mword is_ipc_page_fault(Mword pfa, Mword error);
  static Mword is_io_bitmap_page_fault(Mword pfa);
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "config.h"
#include "mem_unit.h"
#include "paging.h"
#include <cassert>

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword pfa, Mword)
{
  return in_kernel(pfa);
}

IMPLEMENT inline
Mword Kmem::is_io_bitmap_page_fault(Mword)
{
  return 0;
}

IMPLEMENTATION [noncont_mem]:

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size)
{
  static Address ndev = 0;

  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  Address phys_end = Mem_layout::round_superpage(phys + size);
  bool needs_remap = false;

  for (Address p = phys_page; p < phys_end; p += Config::SUPERPAGE_SIZE)
    {
      if (phys_to_pmem(p) == ~0UL)
        {
          needs_remap = true;
          break;
        }
    }

  if (needs_remap)
    {
      for (Address p = phys_page; p < phys_end; p += Config::SUPERPAGE_SIZE)
        {
          Address dm = Mem_layout::Registers_map_start + ndev;
          assert(dm < Mem_layout::Registers_map_end);

          ndev += Config::SUPERPAGE_SIZE;

          auto m = kdir->walk(Virt_addr(dm), K_pte_ptr::Super_level);
          assert (!m.is_valid());
          assert (m.page_order() == Config::SUPERPAGE_SHIFT);
          m.set_page(m.make_page(Phys_mem_addr(p),
                                 Page::Attr(Page::Rights::RWX(),
                                            Page::Type::Uncached(),
                                            Page::Kern::Global())));

          m.write_back_if(true, Mem_unit::Asid_kernel);
          add_pmem(p, dm, Config::SUPERPAGE_SIZE);
        }
    }

  return phys_to_pmem(phys);
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

#include "kmem_space.h"

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size)
{
  Address start = phys & ~63UL;
  Address end = (phys + size - 1U) | 63U;

  // Attention: this only works before the first user space task is created.
  // Otherwise the mpu regions will collide!
  auto diff = (*kdir)->add(
    start, end,
    Mpu_region_attr::make_attr(L4_fpage::Rights::RW(),
                               L4_msg_item::Memory_type::Uncached()));
  assert(!(diff & Mpu_regions::Error));
  Mpu::sync(*kdir, diff);
  Mem::isb();

  return phys;
}

PUBLIC static
void
Kmem::mmio_unmap(Address virt, Address size)
{
  Mem::dsb();

  Address start = virt & ~63UL;
  Address end = (virt + size - 1U) | 63U;

  auto diff = (*kdir)->del(start, end, nullptr);
  Mpu::sync(*kdir, diff);
  Mem::isb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu && !mpu]:

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address /*size*/)
{
  return phys;
}

PUBLIC static
void
Kmem::mmio_unmap(Address, Address)
{}
