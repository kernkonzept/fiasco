INTERFACE [arm]:

#include "kip.h"
#include "mem_layout.h"
#include "paging.h"

class Device_map
{
};

class Kmem : public Mem_layout
{
public:
  static Device_map dev_map;

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

PUBLIC
template< typename T >
T *
Device_map::map(T *phys, bool cache = true)
{ return (T*)Kmem::mmio_remap((Address)phys, Config::SUPERPAGE_SIZE, cache); }

IMPLEMENT inline
Mword Kmem::is_io_bitmap_page_fault(Mword)
{
  return 0;
}

PRIVATE static
bool
Kmem::cont_mapped(Address phys_beg, Address phys_end, Address virt)
{
  for (Address p = phys_beg, v = virt;
       p < phys_end && v < Mem_layout::Registers_map_end;
       p += Config::SUPERPAGE_SIZE, v += Config::SUPERPAGE_SIZE)
    {
      auto e = kdir->walk(Virt_addr(v), K_pte_ptr::Super_level);
      if (!e.is_valid() || p != e.page_addr())
        return false;
    }

  return true;
}

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size, bool cache = false)
{
  static Address ndev = 0;
  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  Address phys_end  = Mem_layout::round_superpage(phys + size);

  for (Address a = Mem_layout::Registers_map_start;
       a < Mem_layout::Registers_map_end; a += Config::SUPERPAGE_SIZE)
    {
      if (cont_mapped(phys_page, phys_end, a))
        return (phys & ~Config::SUPERPAGE_MASK) | (a & Config::SUPERPAGE_MASK);
    }

  static_assert((Mem_layout::Registers_map_start & ~Config::SUPERPAGE_MASK) == 0);
  Address map_addr = Mem_layout::Registers_map_start + ndev;

  for (Address p = phys_page; p < phys_end; p+= Config::SUPERPAGE_SIZE)
    {
      Address dm = Mem_layout::Registers_map_start + ndev;
      assert(dm < Mem_layout::Registers_map_end);

      ndev += Config::SUPERPAGE_SIZE;

      auto m = kdir->walk(Virt_addr(dm), K_pte_ptr::Super_level);
      assert (!m.is_valid());
      assert (m.page_order() == Config::SUPERPAGE_SHIFT);
      m.set_page(m.make_page(Phys_mem_addr(p),
                             Page::Attr(Page::Rights::RW(),
                                        cache ? Page::Type::Normal()
                                              : Page::Type::Uncached(),
                                        Page::Kern::Global())));

      m.write_back_if(true, Mem_unit::Asid_kernel);
    }

  return (phys & ~Config::SUPERPAGE_MASK) | map_addr;
}
//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword pfa, Mword)
{
  return in_kernel(pfa);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "paging.h"

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword, Mword ec)
{
  return !PF::is_usermode_error(ec);
}
