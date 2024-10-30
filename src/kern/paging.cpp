INTERFACE:

#include "types.h"
#include "l4_msg_item.h"
#include "ptab_base.h"
#include "mem_layout.h"

class Page
{
public:

  /* These things must be defined in arch part in
     the most efficient way according to the architecture.

  enum Attribs_enum {
    USER_NO  = xxx, ///< User No access
    USER_RO  = xxx, ///< User Read only
    USER_RW  = xxx, ///< User Read/Write
    USER_RX  = xxx, ///< User Read/Execute
    USER_XO  = xxx, ///< User Execute only
    USER_RWX = xxx, ///< User Read/Write/Execute

    NONCACHEABLE = xxx, ///< Caching is off
    CACHEABLE    = xxx, ///< Caching is enabled
  };

  */

  typedef L4_fpage::Rights Rights;
  typedef L4_msg_item::Memory_type Type;

  struct Kern
  : cxx::int_type_base<unsigned char, Kern>,
    cxx::int_bit_ops<Kern>,
    cxx::int_null_chk<Kern>
  {
    Kern() = default;
    explicit constexpr Kern(Value v) : cxx::int_type_base<unsigned char, Kern>(v) {}

    static constexpr Kern None() { return Kern(0); }
    static constexpr Kern Global() { return Kern(1); }
  };

  struct Flags
  : cxx::int_type_base<unsigned char, Flags>,
    cxx::int_bit_ops<Flags>,
    cxx::int_null_chk<Flags>
  {
    Flags() = default;
    explicit constexpr Flags(Value v) : cxx::int_type_base<unsigned char, Flags>(v) {}

    static constexpr Flags None() { return Flags(0); }
    static constexpr Flags Referenced() { return Flags(1); }
    static constexpr Flags Dirty() { return Flags(2); }
    static constexpr Flags Touched() { return Referenced() | Dirty(); }
  };

  struct Attr
  {
    Rights rights;
    Type type;
    Kern kern;
    Flags flags;

    Attr() = default;
    explicit constexpr Attr(Rights r, Type t, Kern k, Flags f)
    : rights(r), type(t), kern(k), flags(f) {}

    // per-space local mapping, "normally" cached
    static constexpr Attr space_local(Rights r)
    { return Attr(r, Type::Normal(), Kern::None(), Flags::None()); }

    // global kernel mapping, "normally" cached
    static constexpr Attr kern_global(Rights r)
    { return Attr(r, Type::Normal(), Kern::Global(), Flags::None()); }

    static constexpr Attr writable()
    { return Attr(Rights::W(), Type::Normal(), Kern::None(), Flags::None()); }

    Attr apply(Attr o) const
    {
      Attr n = *this;
      n.rights &= o.rights;

      if ((o.type & Type::Set()) == Type::Set())
        n.type = o.type & ~Type::Set();

      return n;
    }

    constexpr bool empty() const
    { return (rights & Rights::RWX()).empty(); }

    Attr operator |= (Attr r)
    {
      rights |= r.rights;
      return *this;
    }
  };
};

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
  enum { Super_level = PTE_PTR::Super_level };
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
