INTERFACE [arm && !cpu_virt]:

#include "paging.h"

typedef Pdir Kpdir;

//---------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

#include "paging.h"

class Kpdir
: public Ptab::Base<K_pte_ptr, Ptab_traits_vpn, Ptab_va_vpn>
{
public:
  enum { Super_level = Pte_ptr::Super_level };
  Address virt_to_phys(Address virt) const
  {
    Virt_addr va(virt);
    auto i = walk(va);
    if (!i.is_valid())
      return ~0;

    return i.page_addr() | cxx::get_lsb(virt, i.page_order());
  }
};

//---------------------------------------------------------------------
INTERFACE [arm]:

#include "mem_layout.h"

class Kmem_space : public Mem_layout
{
public:
  static void init();
  static void init_hw();
  static Kpdir *kdir();

private:
  static Kpdir *_kdir;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT inline
Kpdir *Kmem_space::kdir()
{ return _kdir; }

