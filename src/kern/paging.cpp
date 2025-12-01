INTERFACE:

#include "types.h"
#include "ptab_base.h"
#include "mem_layout.h"

class PF
{
public:
  static Mword is_translation_error( Mword error );
  static Mword is_usermode_error( Mword error );
  static Mword is_read_error( Mword error );
  static Mword addr_to_msgword0( Address pfa, Mword error );
  static Mword pc_to_msgword1( Address pc, Mword error );
};


template<typename ALLOC>
class Pdir_alloc_simple
{
public:
  Pdir_alloc_simple(ALLOC *a = 0) : _a(a) {}

  void *alloc(Bytes size) const
  { return _a->alloc(size); }

  void free(void *block, Bytes size) const
  { _a->free(size, block); }

  bool valid() const { return _a; }

  Phys_mem_addr::Value
  to_phys(void *v) const { return _a->to_phys(v); }

private:
  ALLOC *_a;
};

template<typename PTE_PTR, typename TRAITS, typename VA>
class Pdir_t : public Ptab::Base<PTE_PTR, TRAITS, VA, Mem_layout>
{
public:
  static constexpr unsigned super_level() { return PTE_PTR::super_level(); }
};

IMPLEMENTATION:
//---------------------------------------------------------------------------

template<typename ALLOC>
inline Pdir_alloc_simple<ALLOC> pdir_alloc(ALLOC *a)
{ return Pdir_alloc_simple<ALLOC>(a); }

IMPLEMENT inline NEEDS[PF::is_usermode_error]
Mword PF::pc_to_msgword1(Address pc, Mword error)
{
  return is_usermode_error(error) ? pc : Invalid_address;
}

PUBLIC template<typename PTE_PTR, typename TRAITS, typename VA>
Phys_mem_addr
Pdir_t<PTE_PTR, TRAITS, VA>::virt_to_phys(void *virt) const
{
  return Phys_mem_addr(virt_to_phys(reinterpret_cast<Address>(virt)));
}

//---------------------------------------------------------------------------
IMPLEMENTATION[!ppc32]:

PUBLIC template<typename PTE_PTR, typename TRAITS, typename VA>
Address
Pdir_t<PTE_PTR, TRAITS, VA>::virt_to_phys(Address virt) const
{
  Virt_addr va(virt);
  auto i = this->walk(va);
  if (!i.is_valid())
    return ~0;

  return i.page_addr() | cxx::get_lsb(virt, i.page_order());
}

//---------------------------------------------------------------------------
IMPLEMENTATION[ppc32]:

PUBLIC template<typename PTE_PTR, typename TRAITS, typename VA>
Address
Pdir_t<PTE_PTR, TRAITS, VA>::virt_to_phys(Address virt) const
{
  auto i = this->walk(Virt_addr(virt));

  //if (!i.is_valid())
    return ~0UL;

#ifdef FIX_THIS
  Address phys;
  Pte_htab::pte_lookup(i.e, &phys);
  return phys | (virt & ~(~0UL << i.page_order()));
#endif
}
