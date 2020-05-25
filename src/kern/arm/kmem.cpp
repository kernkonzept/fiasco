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

