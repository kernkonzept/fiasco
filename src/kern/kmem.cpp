INTERFACE:

#include "mem_layout.h"
#include "paging.h"

class Kmem : public Mem_layout
{
public:
  static bool is_kmem_page_fault(Mword pfa, Mword error);
  static bool is_io_bitmap_page_fault(Mword pfa);

  static Kpdir *kdir;
};

IMPLEMENTATION:

#include "config.h"
#include "paging_bits.h"

IMPLEMENT_DEFAULT inline
bool
Kmem::is_io_bitmap_page_fault(Mword)
{ return false; }

IMPLEMENTATION [arm || ia32 || amd64]:

PRIVATE static
bool
Kmem::cont_mapped(Address phys_beg, Address phys_end, Address virt, bool cache)
{
  for (Address p = phys_beg, v = virt;
       p < phys_end && v < Mem_layout::Registers_map_end;
       p += Config::SUPERPAGE_SIZE, v += Config::SUPERPAGE_SIZE)
    {
      auto e = kdir->walk(Virt_addr(v), kdir->Super_level);
      if (!e.is_valid() || p != e.page_addr())
        return false;
      (void)cache;
      assert(   (!cache && e.attribs().type == Page::Type::Uncached())
             || (cache && e.attribs().type == Page::Type::Normal()));
    }

  return true;
}


// Note: Not thread-safe.
PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size, bool cache = false, bool exec = false)
{
  static Address ndev = 0;
  Address phys_page = Super_pg::trunc(phys);
  Address phys_end  = Super_pg::round(phys + size);

  for (Address a = Mem_layout::Registers_map_start;
       a < Mem_layout::Registers_map_end; a += Config::SUPERPAGE_SIZE)
    {
      if (cont_mapped(phys_page, phys_end, a, cache))
        return Super_pg::trunc(a) | Super_pg::offset(phys);
    }

  static_assert(Super_pg::aligned(Mem_layout::Registers_map_start),
                "Registers_map_start must be superpage-aligned");
  Address map_addr = Mem_layout::Registers_map_start + ndev;

  for (Address p = phys_page; p < phys_end; p+= Config::SUPERPAGE_SIZE)
    {
      Address dm = Mem_layout::Registers_map_start + ndev;
      assert(dm < Mem_layout::Registers_map_end);

      ndev += Config::SUPERPAGE_SIZE;

      auto m = kdir->walk(Virt_addr(dm), kdir->Super_level);
      assert (!m.is_valid());
      assert (m.page_order() == Config::SUPERPAGE_SHIFT);
      m.set_page(m.make_page(Phys_mem_addr(p),
                             Page::Attr(exec ? Page::Rights::RWX()
                                             : Page::Rights::RW(),
                                        cache ? Page::Type::Normal()
                                              : Page::Type::Uncached(),
                                        Page::Kern::Global())));

      m.write_back_if(true);
      Mem_unit::tlb_flush_kernel(dm);
    }

  return map_addr | Super_pg::offset(phys);
}
