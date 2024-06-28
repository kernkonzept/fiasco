INTERFACE[riscv]:

#include "mem_unit.h"
#include "types.h"
#include "ptab_base.h"

//----------------------------------------------------------------------------
INTERFACE[riscv && 32bit]:
typedef Ptab::Tupel< Ptab::Traits< Unsigned32, 22, 10, true, false>,
                     Ptab::Traits< Unsigned32, 12, 10, true> >::List Ptab_traits;

class Pte_ptr
{
public:
  enum
  {
    Super_level = 0,
  };
};

//----------------------------------------------------------------------------
INTERFACE[riscv && riscv_sv39]:
typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 9, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

class Pte_ptr
{
public:
  enum
  {
    Super_level = 1,
  };
};

//----------------------------------------------------------------------------
INTERFACE[riscv && riscv_sv48]:
typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 39, 9, true>,
                     Ptab::Traits< Unsigned64, 30, 9, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

class Pte_ptr
{
public:
  enum
  {
    Super_level = 2,
  };
};

//----------------------------------------------------------------------------
INTERFACE[riscv]:
typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;

EXTENSION class Pte_ptr
{
public:
  enum : Mword
  {
    Pte_valid    = 1u << 0,
    Pte_r        = 1u << 1,
    Pte_w        = 1u << 2,
    Pte_x        = 1u << 3,
    Pte_user     = 1u << 4,
    Pte_global   = 1u << 5,
    Pte_accessed = 1u << 6,
    Pte_dirty    = 1u << 7,

    // Each leaf PTE contains an accessed and dirty bit (access flags), but we
    // do not use them for the following reasons.
    //
    // Performance:
    //  "If the supervisor software does not rely on accessed and/or dirty bits,
    //  ... it should always set them to 1 in the PTE to improve performance."
    //
    // Platform-specific availability:
    //  Another reason besides the improved performance is that RISC-V
    //  implementations can choose to raise a page fault if the accessed bit or
    //  dirty bit is not set, instead of setting it. Therefore, we would have
    //  to enable or disable this feature on a per-platform basis.
    Pte_default_leaf = Pte_valid | Pte_accessed | Pte_dirty,

    // "For non-leaf PTEs, the D, A, and U bits are reserved for future standard
    // use. ... they must be cleared by software for forward compatibility."
    Pte_default_non_leaf = Pte_valid,
  };

  enum : Mword
  {
    Pte_attribs_size = 10,
    Pte_ppn_shift    = 12 - Pte_attribs_size,
    Pte_leaf_mask    = Pte_r | Pte_w | Pte_x,
  };

  enum Attribs_enum
  {
    CACHEABLE     = 0, // Not supported
    BUFFERED      = 0, // Not supported
    NONCACHEABLE  = 0, // Not supported
  };

  typedef Mword Entry;

  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level)
  : pte(static_cast<Mword *>(p)), level(level) {}

  Entry *pte;
  unsigned char level;
};

typedef Pdir_t<Pte_ptr, Ptab_traits_vpn, Ptab_va_vpn> Pdir;
class Kpdir : public Pdir {};

EXTENSION class PF
{
public:
  enum : Mword
  {
    Err_inst         = 1u << 0,
    Err_load         = 1u << 1,
    Err_store        = 1u << 2,
    Err_access_fault = 1u << 3,
    Err_usermode     = 1u << 4,
  };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "atomic.h"

PRIVATE static inline ALWAYS_INLINE
Mword
Pte_ptr::phys_to_pte_ppn(Mword phys_addr)
{
  return cxx::mask_lsb(phys_addr, 12) >> Pte_ppn_shift;
}

PRIVATE static inline ALWAYS_INLINE
Mword
Pte_ptr::pte_ppn_to_phys(Mword pte_ppn)
{
  return cxx::mask_lsb(pte_ppn, Pte_attribs_size) << Pte_ppn_shift;
}

PUBLIC inline ALWAYS_INLINE
bool
Pte_ptr::is_valid() const
{
  return access_once(pte) & Pte_valid;
}

PUBLIC inline ALWAYS_INLINE
bool
Pte_ptr::is_leaf() const
{
  return access_once(pte) & Pte_leaf_mask;
}

PUBLIC inline
Pte_ptr::Entry
Pte_ptr::entry() const
{
  return access_once(pte);
}

/**
 * \pre is_leaf() == false
 */
PUBLIC inline ALWAYS_INLINE NEEDS[Pte_ptr::pte_ppn_to_phys]
Mword
Pte_ptr::next_level() const
{
  return pte_ppn_to_phys(access_once(pte));
}

/**
 * \pre cxx::get_lsb(phys_addr, Config::PAGE_SHIFT) == 0
 */
PUBLIC inline ALWAYS_INLINE NEEDS[Pte_ptr::phys_to_pte_ppn]
void
Pte_ptr::set_next_level(Mword phys_addr)
{
  write_now(pte, phys_to_pte_ppn(phys_addr) | Pte_default_non_leaf);
}

PUBLIC inline ALWAYS_INLINE NEEDS[Pte_ptr::phys_to_pte_ppn]
void
Pte_ptr::set_page(Mword phys, Mword attr)
{
  write_now(pte, phys_to_pte_ppn(phys) | attr | Pte_default_leaf);
}

PUBLIC inline
unsigned char
Pte_ptr::page_order() const
{
  return Pdir::page_order_for_level(level);
}

PUBLIC inline NEEDS[Pte_ptr::pte_ppn_to_phys]
Mword
Pte_ptr::page_addr() const
{
  return pte_ppn_to_phys(access_once(pte));
}

PUBLIC static inline ALWAYS_INLINE
Mword
Pte_ptr::make_attribs(Page::Attr attr)
{
  Mword r = Pte_default_leaf;

  if (attr.rights & Page::Rights::R()) r |= Pte_r;
  if (attr.rights & Page::Rights::W()) r |= Pte_r | Pte_w; // w without r not allowed
  if (attr.rights & Page::Rights::X()) r |= Pte_x;
  if (attr.rights & Page::Rights::U()) r |= Pte_user;

  // if (attr.type == Page::Type::Normal()) r |= Page::CACHEABLE;
  // if (attr.type == Page::Type::Buffered()) r |= Page::BUFFERED;
  // if (attr.type == Page::Type::Uncached()) r |= Page::NONCACHEABLE;

  if (attr.kern & Page::Kern::Global()) r |= Pte_global;

  return r;
}

PUBLIC inline
void
Pte_ptr::set_attribs(Page::Attr attr)
{
  write_now(pte,
    cxx::mask_lsb(access_once(pte), Pte_attribs_size) | make_attribs(attr));
}

PUBLIC static inline ALWAYS_INLINE
NEEDS[Pte_ptr::phys_to_pte_ppn, Pte_ptr::make_attribs]
Mword
Pte_ptr::make_page(Phys_mem_addr addr, Page::Attr attr)
{
  Mword pte_ppn = phys_to_pte_ppn(cxx::int_value<Phys_mem_addr>(addr));
  return pte_ppn | make_attribs(attr);
}

PUBLIC inline ALWAYS_INLINE
void
Pte_ptr::set_page(Entry p)
{
  write_now(pte, p);
}

PUBLIC inline ALWAYS_INLINE
void
Pte_ptr::set_page(Phys_mem_addr addr, Page::Attr attr)
{
  set_page(make_page(addr, attr));
}

PUBLIC inline
Page::Attr
Pte_ptr::attribs() const
{
  Mword _raw = access_once(pte);

  Page::Rights r(0);
  if (_raw & Pte_r) r |= Page::Rights::R();
  if (_raw & Pte_w) r |= Page::Rights::W();
  if (_raw & Pte_x) r |= Page::Rights::X();
  if (_raw & Pte_user) r |= Page::Rights::U();

  Page::Kern k(0);
  if (_raw & Pte_global) k |= Page::Kern::Global();

  return Page::Attr(r, Page::Type::Normal(), k, Page::Flags::None());
}

PUBLIC inline
Page::Flags
Pte_ptr::access_flags() const
{
  return Page::Flags::None();
}

PUBLIC inline
void
Pte_ptr::del_rights(Page::Rights r)
{
  if (r & Page::Rights::W())
    write_now(pte, access_once(pte) & ~Pte_w);

  if (r & Page::Rights::X())
    write_now(pte, access_once(pte) & ~Pte_x);
}

PUBLIC inline ALWAYS_INLINE
void
Pte_ptr::clear()
{
  write_now(pte, 0);
}

PUBLIC static inline ALWAYS_INLINE
void
Pte_ptr::write_back(void *, void *)
{}

PUBLIC static inline ALWAYS_INLINE
void
Pte_ptr::write_back_if(bool)
{}

IMPLEMENT inline
Mword
PF::is_translation_error(Mword error)
{
  return !(error & PF::Err_access_fault);
}

IMPLEMENT inline
Mword
PF::is_usermode_error(Mword error)
{
  return error & PF::Err_usermode;
}

IMPLEMENT inline
Mword
PF::is_read_error(Mword error)
{
  // Instruction faults also count as read errors
  return !(error & PF::Err_store);
}

PUBLIC static inline
Mword
PF::is_instruction_error(Mword error)
{
  return error & PF::Err_inst;
}

IMPLEMENT inline
NEEDS[PF::is_translation_error, PF::is_read_error, PF::is_instruction_error]
Mword
PF::addr_to_msgword0(Address pfa, Mword error)
{
  Mword a = pfa & ~7;
  if (is_translation_error(error))
    a |= 1;

  if (!is_read_error(error))
    a |= 2;

  if (is_instruction_error(error))
    a |= 4;

  return a;
}
