INTERFACE [arm]:

#include "mem_unit.h"

class PF {};
class Page
{
};


//-------------------------------------------------------------------------------------
INTERFACE [arm && !arm_lpae]:

class K_pte_ptr
{
public:
  typedef Mword Entry;
  enum
  {
    Super_level    = 0,
  };

  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level) : pte((Mword*)p), level(level) {}

  bool is_valid() const { return *pte & 3; }
  void clear() { *pte = 0; }
  bool is_leaf() const
  {
    switch (level)
      {
      case 0: return (*pte & 3) == 2;
      default: return true;
      };
  }

  Mword next_level() const
  {
    // 1KB second level tables
    return cxx::mask_lsb(*pte, 10);
  }

  void set_next_level(Mword phys)
  {
    write_now(pte, phys | 1);
  }

  unsigned char page_order() const
  {
    if (level == 0)
      return 20; // 1MB
    else
      { // no tiny pages
        if ((*pte & 3) == 1)
          return 16;
        else
          return 12;
      }
  }

  Mword page_addr() const
  { return cxx::mask_lsb(*pte, page_order()); }

  Entry *pte;
  unsigned char level;
};

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits      = 0,
    Mair0_prrr_bits = 0xff0a0028,
    Mair1_nmrr_bits = 0x00100010,
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

class K_pte_ptr
{
public:
  typedef Unsigned64 Entry;

  enum
  {
    Super_level    = 1,
  };

  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level) : pte((Unsigned64*)p), level(level) {}

  bool is_valid() const { return *pte & 1; }
  void clear() { *pte = 0; }
  bool is_leaf() const
  {
    if (level >= 2)
      return true;
    return (*pte & 3) == 1;
  }

  Unsigned64 next_level() const
  {
    // 1KB second level tables
    return cxx::get_lsb(cxx::mask_lsb(*pte, 12), 40);
  }

  void set_next_level(Unsigned64 phys)
  {
    write_now(pte, phys | 3);
  }

  Mword page_addr() const
  { return cxx::mask_lsb(*pte, page_order()); }

  Entry *pte;
  unsigned char level;
};

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (1 << 31) // EAE
                 | (3 << 12) // SH0
                 | (1 << 10) // ORGN0
                 | (1 << 8), // IRGN0
    Mair0_prrr_bits = 0x00ff4400,
    Mair1_nmrr_bits = 0,
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC inline
unsigned char
K_pte_ptr::page_order() const
{ return Pdir::page_order_for_level(level); }


//-----------------------------------------------------------------------------
INTERFACE [arm && armv5]:

#include "types.h"

EXTENSION class Page
{
public:
  typedef Unsigned32 Attribs;

  enum Attribs_enum
  {
    Cache_mask    = 0x0c,
    NONCACHEABLE  = 0x00, ///< Caching is off
    CACHEABLE     = 0x0c, ///< Cache is enabled

    // The next are ARM specific
    WRITETHROUGH = 0x08, ///< Write through cached
    BUFFERED     = 0x04, ///< Write buffer enabled
  };

  enum Default_entries : Mword
  {
    Section_cachable = 0x40e,
    Section_no_cache = 0x402,
    Section_local    = 0,
    Section_global   = 0,
  };
};

EXTENSION class K_pte_ptr
{
  // we have virtually tagged caches so need a cache flush before enabling
  // a page table
  enum { Need_cache_clean = false };
};

//---------------------------------------------------------------------------
INTERFACE [arm && armv6plus && (mpcore || armca9)]:

EXTENSION class Page
{
public:
  enum
  {
    Section_shared = 1UL << 16,
    Mp_set_shared = 0x400,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && armv6plus && !(mpcore || armca9)]:

EXTENSION class Page
{
public:
  enum
  {
    Section_shared = 0,
    Mp_set_shared = 0,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && (armv5 || (armv6 && !mpcore))]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0 }; };

//---------------------------------------------------------------------------
INTERFACE [arm && mpcore]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0xa }; };

//---------------------------------------------------------------------------
INTERFACE [arm && armca9 && !arm_lpae]:

// S Sharable | RGN = Outer WB-WA | IRGN = Inner WB-WA | NOS
EXTENSION class Page
{ public: enum { Ttbr_bits = 0x6a }; };

//---------------------------------------------------------------------------
INTERFACE [arm && armca9 && arm_lpae]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0 }; };

//---------------------------------------------------------------------------
INTERFACE [arm && armca8]: // armv7 w/o multiprocessing ext.

EXTENSION class Page
{ public: enum { Ttbr_bits = 0x2b }; };

//----------------------------------------------------------------------------
INTERFACE [arm && armv6 && !mpcore]:

EXTENSION class Page
{
public:
  enum Attribs_enum
  {
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x144, ///< Cache is enabled
    BUFFERED      = 0x040, ///< Write buffer enabled -- Normal, non-cached
  };

  enum Default_entries : Mword
  {
    Section_cachable_bits = 0x5004,
  };
};

//----------------------------------------------------------------------------
INTERFACE [arm && ((armv6 && mpcore) || (armv7 && !arm_lpae))]:

EXTENSION class Page
{
public:
  enum Attribs_enum
  {
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x008, ///< Cache is enabled
    BUFFERED      = 0x004, ///< Write buffer enabled -- Normal, non-cached
  };

  enum Default_entries : Mword
  {
    Section_cachable_bits = 8,
  };
};

//----------------------------------------------------------------------------
INTERFACE [arm && (armv6 || armv7) && !arm_lpae]:

#include "types.h"

EXTENSION class Page
{
public:
  enum
  {
    Cache_mask    = 0x1cc,
  };

  enum : Mword
  {
    Section_cache_mask = 0x700c,
    Section_local      = (1 << 17),
    Section_global     = 0,
    Section_cachable   = 0x0402 | Section_shared | Section_cachable_bits,
    Section_no_cache   = 0x0402 | Section_shared,
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && !arm_lpae]:

#include "ptab_base.h"

typedef Ptab::List< Ptab::Traits< Unsigned32, 20, 12, true>,
                    Ptab::Traits< Unsigned32, 12, 8, true> > Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;

//-----------------------------------------------------------------------------
INTERFACE [arm && arm_lpae && !hyp]:

#include "ptab_base.h"
#include "types.h"

EXTENSION class Page
{
public:
  enum Attribs_enum
  {
    Cache_mask    = 0x01c,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x008, ///< Cache is enabled
    BUFFERED      = 0x004, ///< Write buffer enabled -- Normal, non-cached
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && arm_lpae && hyp]:

#include "ptab_base.h"
#include "types.h"

EXTENSION class Page
{
public:
  enum Attribs_enum
  {
    Cache_mask    = 0x03c,
    NONCACHEABLE  = 0x000, ///< Caching is off
    CACHEABLE     = 0x03c, ///< Cache is enabled
    BUFFERED      = 0x004, ///< Write buffer enabled -- Normal, non-cached
  };
};


//-----------------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 2, true>,
                     Ptab::Traits< Unsigned64, 21, 9, true>,
                     Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && armv5]:

PUBLIC static inline
bool
K_pte_ptr::need_cache_write_back(bool current_pt)
{ return current_pt; }

PUBLIC inline
void
K_pte_ptr::write_back_if(bool current_pt, Mword /*asid*/ = 0)
{
  if (current_pt)
    Mem_unit::clean_dcache(pte);
}

PUBLIC static inline
void
K_pte_ptr::write_back(void *start, void *end)
{ Mem_unit::clean_dcache(start, end); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (armv6 || armca8)]:

PUBLIC static inline
bool
K_pte_ptr::need_cache_write_back(bool)
{ return true; }

PUBLIC inline
void
K_pte_ptr::write_back_if(bool, Mword asid = Mem_unit::Asid_invalid)
{
  Mem_unit::clean_dcache(pte);
  if (asid != Mem_unit::Asid_invalid)
    Mem_unit::tlb_flush(asid);
}

PUBLIC static inline
void
K_pte_ptr::write_back(void *start, void *end)
{ Mem_unit::clean_dcache(start, end); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && armca9]:

PUBLIC static inline
bool
K_pte_ptr::need_cache_write_back(bool)
{ return false; }

PUBLIC inline
void
K_pte_ptr::write_back_if(bool, Mword asid = Mem_unit::Asid_invalid)
{
  if (asid != Mem_unit::Asid_invalid)
    Mem_unit::tlb_flush(asid);
}

PUBLIC static inline
void
K_pte_ptr::write_back(void *, void *)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && armv5]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) & 0x04) && ((error & 0x0d) == 0x001); }

PRIVATE inline
K_pte_ptr::Entry
K_pte_ptr::_attribs_mask() const
{
  if (level == 0)
    return ~Entry(0x00000c0c);
  else
    return ~Entry(0x00000ffc);
}

PRIVATE inline
Mword
K_pte_ptr::_attribs(Page::Attr attr) const
{
  static const unsigned short perms[] = {
      0x1 << 10, // 0000: none
      0x1 << 10, // 000X: kernel RW (there is no RO)
      0x1 << 10, // 00W0:
      0x1 << 10, // 00WX:

      0x1 << 10, // 0R00:
      0x1 << 10, // 0R0X:
      0x1 << 10, // 0RW0:
      0x1 << 10, // 0RWX:

      0x1 << 10, // U000:
      0x0 << 10, // U00X:
      0x3 << 10, // U0W0:
      0x3 << 10, // U0WX:

      0x0 << 10, // UR00:
      0x0 << 10, // UR0X:
      0x3 << 10, // URW0:
      0x3 << 10  // URWX:
  };

  typedef Page::Type T;
  Mword r = 0;
  if (attr.type == T::Normal())   r |= Page::CACHEABLE;
  if (attr.type == T::Buffered()) r |= Page::BUFFERED;
  if (attr.type == T::Uncached()) r |= Page::NONCACHEABLE;
  if (level == 0)
    return r | perms[cxx::int_value<L4_fpage::Rights>(attr.rights)];
  else
    {
      Mword p = perms[cxx::int_value<L4_fpage::Rights>(attr.rights)];
      p |= p >> 2;
      p |= p >> 4;
      return r | p;
    }
}

PUBLIC inline
Page::Attr
K_pte_ptr::attribs() const
{
  auto r = access_once(pte);
  auto c = r & 0xc;
  r &= 0xc00;

  typedef L4_fpage::Rights R;
  typedef Page::Type T;

  R rights;
  switch (r)
    {
    case 0x000: rights = R::URX(); break;
    default:
    case 0x400: rights = R::RWX(); break;
    case 0xc00: rights = R::URWX(); break;
    }

  T type;
  switch (c)
    {
    default:
    case Page::CACHEABLE: type = T::Normal(); break;
    case Page::BUFFERED:  type = T::Buffered(); break;
    case Page::NONCACHEABLE: type = T::Uncached(); break;
    }
  return Page::Attr(rights, type);
}

PUBLIC inline NEEDS[K_pte_ptr::_attribs, K_pte_ptr::_attribs_mask]
void
K_pte_ptr::set_attribs(Page::Attr attr)
{
  Mword p = access_once(pte);
  p = (p & _attribs_mask()) | _attribs(attr);
  write_now(pte, p);
}

PUBLIC inline NEEDS[K_pte_ptr::_attribs]
void
K_pte_ptr::create_page(Phys_mem_addr addr, Page::Attr attr)
{
  Mword p = 2 | _attribs(attr) | cxx::int_value<Phys_mem_addr>(addr);
  write_now(pte, p);
}

PUBLIC inline
bool
K_pte_ptr::add_attribs(Page::Attr attr)
{
  typedef L4_fpage::Rights R;

  if (attr.rights & R::W())
    {
      auto p = access_once(pte);
      if ((p & 0xc00) == 0x000)
        {
          p |= level == 0 ? 0xc00 : 0xff0;
          write_now(pte, p);
          return true;
        }
    }
  return false;
}

PUBLIC inline
Page::Rights
K_pte_ptr::access_flags() const
{ return Page::Rights(0); }

PUBLIC inline
void
K_pte_ptr::del_rights(L4_fpage::Rights r)
{
  if (!(r & L4_fpage::Rights::W()))
    return;

  auto p = access_once(pte);
  if ((p & 0xc00) == 0xc00)
    {
      p &= (level == 0) ? ~Mword(0xc00) : ~Mword(0xff0);
      write_now(pte, p);
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae && (armv6 || armv7)]:

PRIVATE inline
K_pte_ptr::Entry
K_pte_ptr::_attribs_mask() const
{
  if (level == 0)
    return ~Entry(0x0000881c);
  else
    return ~Entry(0x0000022d);
}

PRIVATE inline
Mword
K_pte_ptr::_attribs(Page::Attr attr) const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;

  Mword lower = Page::Mp_set_shared;
  if (attr.type == T::Normal())   lower |= Page::CACHEABLE;
  if (attr.type == T::Buffered()) lower |= Page::BUFFERED;
  if (attr.type == T::Uncached()) lower |= Page::NONCACHEABLE;
  Mword upper = lower & ~0x0f;
  lower &= 0x0f;

  if (!(attr.kern & K::Global()))
    upper |= 0x800;

  if (!(attr.rights & R::W()))
    upper |= 0x200;

  if (attr.rights & R::U())
    upper |= 0x20;

  if (!(attr.rights & R::X()))
    {
      if (level == 0)
        lower |= 0x10;
      else
        lower |= 0x01;
    }

  if (level == 0)
    return lower | (upper << 6);
  else
    return lower | upper;
}

PUBLIC inline
Page::Attr
K_pte_ptr::attribs() const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;

  auto c = access_once(pte);

  R rights = R::R();

  if (level == 0)
    {
      if (!(c & 0x10))
        rights |= R::X();

      c = (c & 0x0f) | ((c >> 6) & 0xfff0);
    }
  else if (!(c & 0x01))
    rights |= R::X();

  if (!(c & 0x200))
    rights |= R::W();
  if (c & 0x20)
    rights |= R::U();

  T type;
  switch (c & Page::Cache_mask)
    {
    default:
    case Page::CACHEABLE: type = T::Normal(); break;
    case Page::BUFFERED:  type = T::Buffered(); break;
    case Page::NONCACHEABLE: type = T::Uncached(); break;
    }

  K k(0);
  if (!(c & 0x800))
    k |= K::Global();

  return Page::Attr(rights, type, k);
}

PUBLIC inline NEEDS[K_pte_ptr::_attribs]
void
K_pte_ptr::create_page(Phys_mem_addr addr, Page::Attr attr)
{
  Mword p = 2 | _attribs(attr) | cxx::int_value<Phys_mem_addr>(addr);
  if (level == 0)
    p |= 0x400;  // AP[0]
  else
    p |= 0x10;   // AP[0]

  write_now(pte, p);
}

PUBLIC inline
bool
K_pte_ptr::add_attribs(Page::Attr attr)
{
  typedef L4_fpage::Rights R;
  Mword n_attr = 0;

  if (attr.rights & R::W())
    {
      if (level == 0)
        n_attr = 0x200 << 6;
      else
        n_attr = 0x200;
    }

  if (attr.rights & R::X())
    {
      if (level == 0)
        n_attr |= 0x10;
      else
        n_attr |= 0x01;
    }


  auto p = access_once(pte);
  if (p & n_attr)
    {
      p &= ~n_attr;
      write_now(pte, p);
      return true;
    }

  return false;
}

PUBLIC inline
Page::Rights
K_pte_ptr::access_flags() const
{ return Page::Rights(0); }

PUBLIC inline
void
K_pte_ptr::del_rights(L4_fpage::Rights r)
{
  Mword n_attr = 0;
  if (r & L4_fpage::Rights::W())
    {
      if (level == 0)
        n_attr = 0x200 << 6;
      else
        n_attr = 0x200;
    }

  if (r & L4_fpage::Rights::X())
    {
      if (level == 0)
        n_attr |= 0x10;
      else
        n_attr |= 0x01;
    }

  if (!n_attr)
    return;

  auto p = access_once(pte);
  if ((p & n_attr) != n_attr)
    {
      p |= n_attr;
      write_now(pte, p);
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && armv7]:

PRIVATE inline
K_pte_ptr::Entry
K_pte_ptr::_attribs_mask() const
{ return ~Entry(0x00400000000008dc); }

PRIVATE inline
K_pte_ptr::Entry
K_pte_ptr::_attribs(Page::Attr attr) const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;

  Entry lower = 0x300; // inner sharable
  if (attr.type == T::Normal())   lower |= Page::CACHEABLE;
  if (attr.type == T::Buffered()) lower |= Page::BUFFERED;
  if (attr.type == T::Uncached()) lower |= Page::NONCACHEABLE;

  if (!(attr.kern & K::Global()))
    lower |= 0x800;

  if (!(attr.rights & R::W()))
    lower |= 0x080;

  if (attr.rights & R::U())
    lower |= 0x040;

  if (!(attr.rights & R::X()))
    lower |= 0x0040000000000000;

  return lower;
}

PUBLIC inline
Page::Attr
K_pte_ptr::attribs() const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;

  auto c = access_once(pte);

  R rights = R::R();
  if (!(c & 0x80))
    rights |= R::W();
  if (c & 0x40)
    rights |= R::U();

  if (!(c & 0x0040000000000000))
    rights |= R::X();

  T type;
  switch (c & Page::Cache_mask)
    {
    default:
    case Page::CACHEABLE: type = T::Normal(); break;
    case Page::BUFFERED:  type = T::Buffered(); break;
    case Page::NONCACHEABLE: type = T::Uncached(); break;
    }

  K k(0);
  if (!(c & 0x800))
    k |= K::Global();

  return Page::Attr(rights, type, k);
}

PUBLIC inline NEEDS[K_pte_ptr::_attribs]
void
K_pte_ptr::create_page(Phys_mem_addr addr, Page::Attr attr)
{
  Entry p = 0x400 | _attribs(attr) | cxx::int_value<Phys_mem_addr>(addr);

  if (level == Pdir::Depth)
    p |= 3;
  else
    p |= 1;

  write_now(pte, p);
}

PUBLIC inline
bool
K_pte_ptr::add_attribs(Page::Attr attr)
{
  typedef L4_fpage::Rights R;

  Entry n_attr = 0;

  if (attr.rights & R::W())
    n_attr = 0x80;

  if (attr.rights & R::X())
    n_attr |= 0x0040000000000000;

  if (!n_attr)
    return false;

  auto p = access_once(pte);
  if (p & n_attr)
    {
      p &= ~n_attr;
      write_now(pte, p);
      return true;
    }
  return false;
}

PUBLIC inline
Page::Rights
K_pte_ptr::access_flags() const
{ return Page::Rights(0); }

PUBLIC inline
void
K_pte_ptr::del_rights(L4_fpage::Rights r)
{
  Entry n_attr = 0;
  if (r & L4_fpage::Rights::W())
    n_attr = 0x80;

  if (r & L4_fpage::Rights::X())
    n_attr |= 0x0040000000000000;

  if (!n_attr)
    return;

  auto p = access_once(pte);
  if ((p & n_attr) != n_attr)
    {
      p |= n_attr;
      write_now(pte, p);
    }
}

//---------------------------------------------------------------------------
INTERFACE [arm && arm_lpae && (armv6 || armv7) && hyp]:

class Pte_ptr : public K_pte_ptr
{
public:
  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : K_pte_ptr(p, level) {}

};


//---------------------------------------------------------------------------
INTERFACE [arm && !hyp]:

typedef K_pte_ptr Pte_ptr;

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && (armv6 || armv7) && hyp]:

PRIVATE inline
Pte_ptr::Entry
Pte_ptr::_attribs_mask() const
{ return ~Entry(0x00400000000000fc); }

PRIVATE inline
Pte_ptr::Entry
Pte_ptr::_attribs(Page::Attr attr) const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;

  Entry lower = 0x300 | (0x1 << 6); // inner sharable, readable
  if (attr.type == T::Normal())   lower |= Page::CACHEABLE;
  if (attr.type == T::Buffered()) lower |= Page::BUFFERED;
  if (attr.type == T::Uncached()) lower |= Page::NONCACHEABLE;

  if (attr.rights & R::W())
    lower |= (0x2 << 6);

  if (!(attr.rights & R::X()))
    lower |= 0x0040000000000000;

  return lower;
}

PUBLIC inline
Page::Attr
Pte_ptr::attribs() const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;

  auto c = access_once(pte);

  R rights = R::R();
  rights |= R::U();

  if (c & (0x2 << 6))
    rights |= R::W();

  if (!(c & 0x0040000000000000))
    rights |= R::X();

  T type;
  switch (c & Page::Cache_mask)
    {
    default:
    case Page::CACHEABLE:    type = T::Normal(); break;
    case Page::BUFFERED:     type = T::Buffered(); break;
    case Page::NONCACHEABLE: type = T::Uncached(); break;
    }

  return Page::Attr(rights, type, K(0));
}

PUBLIC inline NEEDS[Pte_ptr::_attribs]
void
Pte_ptr::create_page(Phys_mem_addr addr, Page::Attr attr)
{
  Entry p = 0x400 | _attribs(attr) | cxx::int_value<Phys_mem_addr>(addr);
  if (level == Pdir::Depth)
    p |= 3;
  else
    p |= 1;

  write_now(pte, p);
}

PUBLIC inline
bool
Pte_ptr::add_attribs(Page::Attr attr)
{
  typedef L4_fpage::Rights R;

  Entry n_attr = 0;
  Mword a_attr = 0;

  if (attr.rights & R::W())
    a_attr = 0x2 << 6;

  if (attr.rights & R::X())
    n_attr |= 0x0040000000000000;

  if (!n_attr && !a_attr)
    return false;

  auto p = access_once(pte);
  auto old = p;
  p &= ~n_attr;
  p |= a_attr;

  if (p != old)
    {
      write_now(pte, p);
      return true;
    }
  return false;
}

PUBLIC inline
Page::Rights
Pte_ptr::access_flags() const
{ return Page::Rights(0); }

PUBLIC inline
void
Pte_ptr::del_rights(L4_fpage::Rights r)
{
  Entry n_attr = 0;
  Mword a_attr = 0;
  if (r & L4_fpage::Rights::W())
    a_attr = 0x2 << 6;

  if (r & L4_fpage::Rights::X())
    n_attr |= 0x0040000000000000;

  if (!n_attr && !a_attr)
    return;

  auto p = access_once(pte);
  auto old = p;
  p |= n_attr;
  p &= ~Entry(a_attr);

  if (p != old)
    write_now(pte, p);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (armv6 || armv7) && !arm_lpae]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) == 0x24) && ((error & 0x40f) == 0x001); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (armv6 || armv7) && arm_lpae]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) == 0x24) && ((error & 0x3f) == 0x21); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (armv6 || armv7)]:

PUBLIC inline NEEDS[K_pte_ptr::_attribs, K_pte_ptr::_attribs_mask]
void
K_pte_ptr::set_attribs(Page::Attr attr)
{
  Entry p = access_once(pte);
  p = (p & _attribs_mask()) | _attribs(attr);
  write_now(pte, p);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

IMPLEMENT inline
Mword PF::is_translation_error(Mword error)
{
  return (error & 0x0d/*FSR_STATUS_MASK*/) == 0x05/*FSR_TRANSL*/;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

IMPLEMENT inline
Mword PF::is_translation_error(Mword error)
{
  return (error & 0x3c) == 0x04;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT inline
Mword PF::is_usermode_error(Mword error)
{
  return !((error >> 26) & 1);
}

IMPLEMENT inline
Mword PF::is_read_error(Mword error)
{
  return !(error & (1 << 6));
}

IMPLEMENT inline NEEDS[PF::is_read_error]
Mword PF::addr_to_msgword0(Address pfa, Mword error)
{
  Mword a = pfa & ~7;
  if (is_translation_error(error))
    a |= 1;
  if (!is_read_error(error))
    a |= 2;
  if (!((error >> 26) & 0x04))
    a |= 4;
  return a;
}
