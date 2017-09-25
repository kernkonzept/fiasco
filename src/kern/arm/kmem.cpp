INTERFACE [arm]:

#include "kip.h"
#include "mem_layout.h"

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
#include "kmem_space.h"
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
Kmem::mmio_remap(Address phys)
{
  static Address ndev = 0;
  Address v = phys_to_pmem(phys);
  if (v != ~0UL)
    return v;

  Address dm = Mem_layout::Registers_map_start + ndev;
  assert(dm < Mem_layout::Registers_map_end);

  ndev += Config::SUPERPAGE_SIZE;

  auto m = Kmem_space::kdir()->walk(Virt_addr(dm), Pte_ptr::Super_level);
  assert (!m.is_valid());
  assert (m.page_order() == Config::SUPERPAGE_SHIFT);
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  m.create_page(Phys_mem_addr(phys_page), Page::Attr(Page::Rights::RWX(), Page::Type::Uncached(),
                Page::Kern::Global()));

  m.write_back_if(true, Mem_unit::Asid_kernel);
  add_pmem(phys_page, dm, Config::SUPERPAGE_SIZE);

  return phys_to_pmem(phys);
}

IMPLEMENTATION [!noncont_mem]:

PUBLIC static
Address
Kmem::mmio_remap(Address phys)
{
  auto m = Kmem_space::kdir()->walk(Virt_addr(phys), Pte_ptr::Super_level);
  if (m.is_valid())
    {
      assert (m.page_order() >= Config::SUPERPAGE_SHIFT);
      assert (m.page_addr() == cxx::mask_lsb(phys, m.page_order()));
      assert (m.attribs().type == Page::Type::Uncached());
      return phys;
    }

  assert (m.page_order() == Config::SUPERPAGE_SHIFT);
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  m.create_page(Phys_mem_addr(phys_page), Page::Attr(Page::Rights::RWX(),
                Page::Type::Uncached(),
                Page::Kern::Global()));

  m.write_back_if(true, Mem_unit::Asid_kernel);

  return phys;
}
