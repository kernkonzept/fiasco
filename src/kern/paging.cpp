INTERFACE:

#include "types.h"
#include "l4_msg_item.h"

EXTENSION class Page
{
public:

  /* These things must be defined in arch part in
     the most efficent way according to the architecture.

  typedef int Attribs;

  enum Attribs_enum {
    USER_NO  = xxx, ///< User No access
    USER_RO  = xxx, ///< User Read only
    USER_RW  = xxx, ///< User Read/Write
    USER_RX  = xxx, ///< User Read/Execute
    USER_XO  = xxx, ///< User Execute only
    USER_RWX = xxx, ///< User Read/Write/Execute

    NONCACHEABLE = xxx, ///< Caching is off
    CACHEABLE    = xxx, ///< Cahe is enabled
  };

  */
  typedef L4_msg_item::Memory_type Type;

  struct Kern
  : cxx::int_type_base<unsigned char, Kern>,
    cxx::int_bit_ops<Kern>,
    cxx::int_null_chk<Kern>
  {
    Kern() = default;
    explicit Kern(Value v) : cxx::int_type_base<unsigned char, Kern>(v) {}

    static Kern Global() { return Kern(1); }
  };

  typedef L4_fpage::Rights Rights;

  struct Attr
  {
    Rights rights;
    Type type;
    Kern kern;

    Attr() = default;
    explicit Attr(Rights r, Type t = Type::Normal(), Kern k = Kern(0))
    : rights(r), type(t), kern(k) {}

    Attr apply(Attr o) const
    {
      Attr n = *this;
      n.rights &= o.rights;
      if ((o.type & Type::Set()) == Type::Set())
        n.type = o.type & ~Type::Set();
      return n;
    }
  };
};

EXTENSION class PF
{
public:
  static Mword is_translation_error( Mword error );
  static Mword is_usermode_error( Mword error );
  static Mword is_read_error( Mword error );
  static Mword addr_to_msgword0( Address pfa, Mword error );
  static Mword pc_to_msgword1( Address pc, Mword error );
};


class Pdir_alloc_null
{
public:
  Pdir_alloc_null() {}
  void *alloc(unsigned long) const { return 0; }
  void free(void *, unsigned long) const {}
  bool valid() const { return false; }
};

template<typename ALLOC>
class Pdir_alloc_simple
{
public:
  Pdir_alloc_simple(ALLOC *a = 0) : _a(a) {}

  void *alloc(unsigned long size) const
  { return _a->unaligned_alloc(size); }

  void free(void *block, unsigned long size) const
  { _a->unaligned_free(size, block); }

  bool valid() const { return _a; }

  Phys_mem_addr::Value
  to_phys(void *v) const { return _a->to_phys(v); }

private:
  ALLOC *_a;
};


class Pdir
: public Ptab::Base<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn >
{
public:
  enum { Super_level = Pte_ptr::Super_level };
};

IMPLEMENTATION:
//---------------------------------------------------------------------------

template<typename ALLOC>
inline Pdir_alloc_simple<ALLOC> pdir_alloc(ALLOC *a)
{ return Pdir_alloc_simple<ALLOC>(a); }

IMPLEMENT inline NEEDS[PF::is_usermode_error]
Mword PF::pc_to_msgword1(Address pc, Mword error)
{
  return is_usermode_error(error) ? pc : (Mword)-1;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[!ppc32]:

PUBLIC
Address
Pdir::virt_to_phys(Address virt) const
{
  Virt_addr va(virt);
  auto i = walk(va);
  if (!i.is_valid())
    return ~0;

  return i.page_addr() | cxx::get_lsb(virt, i.page_order());
}

//---------------------------------------------------------------------------
IMPLEMENTATION[ppc32]:

PUBLIC
Address
Pdir::virt_to_phys(Address virt) const
{
  auto i = walk(Virt_addr(virt));

  //if (!i.is_valid())
    return ~0UL;

#ifdef FIX_THIS
  Address phys;
  Pte_htab::pte_lookup(i.e, &phys);
  return phys | (virt & ~(~0UL << i.page_order()));
#endif
}
