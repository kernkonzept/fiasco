INTERFACE [sparc]:

#include "paging.h"

class Page_table;

class Kmem : public Mem_layout
{
public:
  static Mword *kernel_sp();
  static void kernel_sp(Mword *);

  static Mword is_kmem_page_fault( Mword pfa, Mword error );
  static Mword is_io_bitmap_page_fault( Mword pfa );

  static Address virt_to_phys(const void *addr);
private:
  static Mword *_sp;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "paging.h"
#include "panic.h"

extern Mword kernel_srmmu_l1[256];
Kpdir *Mem_layout::kdir = (Kpdir *)kernel_srmmu_l1;
Mword *Kmem::_sp = 0;

PUBLIC static
Address
Kmem::mmio_remap(Address phys, Address size)
{
  static Address ndev = 0;

  Address phys_page = cxx::mask_lsb(phys, Config::SUPERPAGE_SHIFT);
  Address phys_end = (phys + size + Config::SUPERPAGE_SIZE - 1) & ~(Config::SUPERPAGE_SIZE-1);
  bool needs_remap = false;

  for (Address p = phys_page; p < phys_end; p += Config::SUPERPAGE_SIZE)
    {
      if (phys_to_pmem(p) == ~0UL)
        needs_remap = true;
    }

  if (needs_remap)
    {
      for (Address p = (phys & ~(Config::SUPERPAGE_SIZE-1));
           p < phys_end; p += Config::SUPERPAGE_SIZE)
        {
          Address dm = Mem_layout::Registers_map_start + ndev;
          assert(dm < Mem_layout::Registers_map_end);

          ndev += Config::SUPERPAGE_SIZE;

          auto m = kdir->walk(Virt_addr(dm), Pte_ptr::Super_level);
          assert (!m.is_valid());
          assert (m.page_order() == 24);
          m.create_page(Phys_mem_addr(p),Page::Attr(Page::Rights::RWX(),
                                                    Page::Type::Uncached(),
                                                    Page::Kern::Global()));

          //m.write_back_if(true, Mem_unit::Asid_kernel);
          add_pmem(p, dm, Config::SUPERPAGE_SIZE);
        }
    }

  return phys_to_pmem(phys);
}

IMPLEMENT inline
Mword *Kmem::kernel_sp()
{ return _sp;}

IMPLEMENT inline
void Kmem::kernel_sp(Mword *sp)
{ _sp = sp; }

IMPLEMENT inline NEEDS["paging.h"]
Address Kmem::virt_to_phys(const void *addr)
{
  Address a = reinterpret_cast<Address>(addr);
  return kdir->virt_to_phys(a);
}

IMPLEMENT inline
Mword Kmem::is_kmem_page_fault(Mword pfa, Mword /*error*/)
{
  return in_kernel(pfa);
}

IMPLEMENT inline
Mword Kmem::is_io_bitmap_page_fault( Mword /*pfa*/ )
{
  return 0;
}
