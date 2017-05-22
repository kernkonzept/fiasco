INTERFACE [arm]:

#include "mem_unit.h"

/**
 * Mixin for PTE pointers for CPUs with virtual caches and without ASIDs.
 * (before and including ARMv5)
 */
template<typename CLASS>
struct Pte_v_cache_no_asid
{
  static bool need_cache_write_back(bool current_pt)
  { return current_pt; }

  void write_back_if(bool current_pt, Mword /*asid*/ = 0)
  {
    if (current_pt)
      Mem_unit::clean_dcache(static_cast<CLASS const *>(this)->pte);
  }

  static void write_back(void *start, void *end)
  {
    Mem_unit::clean_dcache(start, end);
  }
};

/**
 * Mixin for PTE pointers for CPUs with ASIDs and non-coherent MMU.
 * (ARMv6 and ARMv7 without multiprocessing extension).
 */
template<typename CLASS>
struct Pte_cache_asid
{
  static bool need_cache_write_back(bool)
  { return true; }

  void write_back_if(bool, Mword asid = Mem_unit::Asid_invalid)
  {
    Mem_unit::clean_dcache(static_cast<CLASS const *>(this)->pte);
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }

  static void write_back(void *start, void *end)
  {
    Mem_unit::clean_dcache(start, end);
  }
};

/**
 * Mixin for PTE pointers for CPUs with ASIDs and coherent MMU.
 * (ARMv7 with multiprocessing extension or LPAE and ARMv8).
 */
template<typename CLASS>
struct Pte_no_cache_asid
{
  static bool need_cache_write_back(bool)
  { return false; }

  static void write_back_if(bool, Mword asid = Mem_unit::Asid_invalid)
  {
    if (asid != Mem_unit::Asid_invalid)
      Mem_unit::tlb_flush(asid);
  }

  static void write_back(void *, void *)
  {}
};

/**
 * Mixin for PTE pointers for ARMv5 page-table attributes.
 */
template<typename CLASS, typename Entry>
class Pte_v5_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Entry _attribs_mask() const
  {
    if (_this()->level == 0)
      return ~Entry(0x00000c0c);
    else
      return ~Entry(0x00000ffc);
  }

  Entry _attribs(Page::Attr attr) const
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
    if (_this()->level == 0)
      return r | perms[cxx::int_value<L4_fpage::Rights>(attr.rights)];
    else
      {
        Mword p = perms[cxx::int_value<L4_fpage::Rights>(attr.rights)];
        p |= p >> 2;
        p |= p >> 4;
        return r | p;
      }
  }

  Entry _page_bits() const
  { return 2; }

  Page::Attr attribs() const
  {
    auto r = access_once(_this()->pte);
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

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    if (!(r & L4_fpage::Rights::W()))
      return;

    auto p = access_once(_this()->pte);
    if ((p & 0xc00) == 0xc00)
      {
        p &= (_this()->level == 0) ? ~Mword(0xc00) : ~Mword(0xff0);
        write_now(_this()->pte, p);
      }
  }
};

/**
 * Mixin for PTE pointers for ARMv6+ page-table attributes
 * (short descriptors).
 */
template<typename CLASS, typename ATTRIBS>
class Pte_v6plus_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Unsigned32 _attribs_mask() const
  {
    if (_this()->level == 0)
      return ~Unsigned32(0x0000881c);
    else
      return ~Unsigned32(0x0000022d);
  }

  Unsigned32 _attribs(Page::Attr attr) const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    Mword lower = ATTRIBS::Mp_set_shared;
    if (attr.type == T::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == T::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == T::Uncached()) lower |= ATTRIBS::NONCACHEABLE;
    Mword upper = lower & ~0x0f;
    upper |= 0x10;  // AP[0]
    lower &= 0x0f;

    if (!(attr.kern & K::Global()))
      upper |= 0x800;

    if (!(attr.rights & R::W()))
      upper |= 0x200;

    if (attr.rights & R::U())
      upper |= 0x20;

    if (!(attr.rights & R::X()))
      {
        if (_this()->level == 0)
          lower |= 0x10;
        else
          lower |= 0x01;
      }

    if (_this()->level == 0)
      return lower | (upper << 6);
    else
      return lower | upper;
  }

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    auto c = access_once(_this()->pte);

    R rights = R::R();

    if (_this()->level == 0)
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
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE: type = T::Normal(); break;
      case ATTRIBS::BUFFERED:  type = T::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = T::Uncached(); break;
      }

    K k(0);
    if (!(c & 0x800))
      k |= K::Global();

    return Page::Attr(rights, type, k);
  }

  Unsigned32 _page_bits() const { return 2; }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    Mword n_attr = 0;
    if (r & L4_fpage::Rights::W())
      {
        if (_this()->level == 0)
          n_attr = 0x200 << 6;
        else
          n_attr = 0x200;
      }

    if (r & L4_fpage::Rights::X())
      {
        if (_this()->level == 0)
          n_attr |= 0x10;
        else
          n_attr |= 0x01;
      }

    if (!n_attr)
      return;

    auto p = access_once(_this()->pte);
    if ((p & n_attr) != n_attr)
      {
        p |= n_attr;
        write_now(_this()->pte, p);
      }
  }
};


/**
 * Mixin for PTE pointers for ARMv6+ LPAE page-table attributes
 * (long descriptors).
 */
template<typename CLASS, typename ATTRIBS>
class Pte_long_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Unsigned64 _attribs_mask() const
  { return ~Unsigned64(0x00400000000008dc); }

  Unsigned64 _attribs(Page::Attr attr) const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    Unsigned64 lower = 0x300; // inner sharable
    if (attr.type == T::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == T::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == T::Uncached()) lower |= ATTRIBS::NONCACHEABLE;

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

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    auto c = access_once(_this()->pte);

    R rights = R::R();
    if (!(c & 0x80))
      rights |= R::W();
    if (c & 0x40)
      rights |= R::U();

    if (!(c & 0x0040000000000000))
      rights |= R::X();

    T type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE: type = T::Normal(); break;
      case ATTRIBS::BUFFERED:  type = T::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = T::Uncached(); break;
      }

    K k(0);
    if (!(c & 0x800))
      k |= K::Global();

    return Page::Attr(rights, type, k);
  }

  Unsigned64 _page_bits() const
  {
    return 0x400 | ((_this()->level == CLASS::Max_level) ? 3 : 1);
  }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    Unsigned64 n_attr = 0;
    if (r & L4_fpage::Rights::W())
      n_attr = 0x80;

    if (r & L4_fpage::Rights::X())
      n_attr |= 0x0040000000000000;

    if (!n_attr)
      return;

    auto p = access_once(_this()->pte);
    if ((p & n_attr) != n_attr)
      {
        p |= n_attr;
        write_now(_this()->pte, p);
      }
  }
};


/**
 * Mixin for PTE pointers for ARMv6+ stage 2 page-table attributes
 */
template<typename CLASS, typename ATTRIBS>
class Pte_stage2_attribs
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  Unsigned64 _attribs_mask() const
  { return ~Unsigned64(0x00400000000000fc); }

  Unsigned64 _attribs(Page::Attr attr) const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;

    Unsigned64 lower = 0x300 | (0x1 << 6); // inner sharable, readable
    if (attr.type == T::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == T::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == T::Uncached()) lower |= ATTRIBS::NONCACHEABLE;

    if (attr.rights & R::W())
      lower |= (0x2 << 6);

    if (!(attr.rights & R::X()))
      lower |= 0x0040000000000000;

    return lower;
  }

  Page::Attr attribs() const
  {
    typedef L4_fpage::Rights R;
    typedef Page::Type T;
    typedef Page::Kern K;

    auto c = access_once(_this()->pte);

    R rights = R::R();
    rights |= R::U();

    if (c & (0x2 << 6))
      rights |= R::W();

    if (!(c & 0x0040000000000000))
      rights |= R::X();

    T type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE:    type = T::Normal(); break;
      case ATTRIBS::BUFFERED:     type = T::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = T::Uncached(); break;
      }

    return Page::Attr(rights, type, K(0));
  }

  Unsigned64 _page_bits() const
  {
    return 0x400 | ((_this()->level == CLASS::Max_level) ? 3 : 1);
  }

  Page::Rights access_flags() const
  { return Page::Rights(0); }

  void del_rights(L4_fpage::Rights r)
  {
    Unsigned64 n_attr = 0;
    Mword a_attr = 0;
    if (r & L4_fpage::Rights::W())
      a_attr = 0x2 << 6;

    if (r & L4_fpage::Rights::X())
      n_attr |= 0x0040000000000000;

    if (!n_attr && !a_attr)
      return;

    auto p = access_once(_this()->pte);
    auto old = p;
    p |= n_attr;
    p &= ~Unsigned64(a_attr);

    if (p != old) {
      write_now(_this()->pte, p); }
  }
};


/**
 * Mixin for PTE pointers for 32bit page tables (short descriptors).
 */
template<typename CLASS>
class Pte_short_desc
{
public:
  enum
  {
    Max_level   = 1,
    Super_level = 0,
  };

  typedef Unsigned32 Entry;

  Unsigned32 *pte;
  unsigned char level;

  Pte_short_desc() = default;
  Pte_short_desc(void *p, unsigned char level)
  : pte((Entry *)p), level(level)
  {}

  bool is_valid() const { return access_once(pte) & 3; }
  void clear() { write_now(pte, 0); }
  bool is_leaf() const
  {
    switch (level)
      {
      case 0: return (access_once(pte) & 3) == 2;
      default: return true;
      };
  }

  Mword next_level() const
  {
    // 1KB second level tables
    return cxx::mask_lsb(access_once(pte), 10);
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

  Unsigned32 page_addr() const
  { return cxx::mask_lsb(*pte, page_order()); }

  Entry entry() const { return *pte; }
};

/**
 * Mixin for PTE pointers for 64bit page tables (long descriptors).
 */
template<typename CLASS>
class Pte_long_desc
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  typedef Unsigned64 Entry;

  Entry *pte;
  unsigned char level;

  Pte_long_desc() = default;
  Pte_long_desc(void *p, unsigned char level)
  : pte((Unsigned64*)p), level(level)
  {}

  bool is_valid() const { return *pte & 1; }
  void clear() { write_now(pte, 0); }
  bool is_leaf() const
  {
    if (level >= CLASS::Max_level)
      return true;
    return (*pte & 3) == 1;
  }

  Unsigned64 next_level() const
  {
    return cxx::get_lsb(cxx::mask_lsb(*pte, 12), 52);
  }

  void set_next_level(Unsigned64 phys)
  {
    write_now(pte, phys | 3);
  }

  Unsigned64 page_addr() const
  { return cxx::get_lsb(cxx::mask_lsb(*pte, _this()->page_order()), 52); }

  Entry entry() const { return *pte; }
};

/**
 * Generic mixin for PTE pointers.
 */
template<typename CLASS, typename Entry>
class Pte_generic
{
private:
  CLASS const *_this() const { return static_cast<CLASS const *>(this); }
  CLASS *_this() { return static_cast<CLASS *>(this); }

public:
  void set_page(Entry p)
  {
    write_now(_this()->pte, p);
  }

  void set_attribs(Page::Attr attr)
  {
    auto p = access_once(_this()->pte);
    p = (p & _this()->_attribs_mask()) | _this()->_attribs(attr);
    write_now(_this()->pte, p);
  }

  Entry make_page(Phys_mem_addr addr, Page::Attr attr)
  {
    return _this()->_page_bits() | _this()->_attribs(attr)
           | cxx::int_value<Phys_mem_addr>(addr);
  }
};

//-------------------------------------------------------------------------------------
INTERFACE [arm && !arm_lpae]:

class K_pte_ptr :
  public Pte_short_desc<K_pte_ptr>,
  public Pte_generic<K_pte_ptr, Unsigned32>
{
public:
  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level)
  : Pte_short_desc(p, level) {}
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

class K_pte_ptr :
  public Pte_long_desc<K_pte_ptr>,
  public Pte_generic<K_pte_ptr, Unsigned64>,
  public Pte_long_attribs<K_pte_ptr, Page>
{
public:
  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level)
  : Pte_long_desc(p, level) {}
};

EXTENSION class Page
{
public:
  enum
  {
    /// Attributes for page-table walks
    Tcr_attribs =  (3UL << 4)  // SH0
                 | (1UL << 2)  // ORGN0
                 | (1UL << 0), // IRGN0

    Mair0_prrr_bits = 0x00ff4400,
    Mair1_nmrr_bits = 0,
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC inline ALWAYS_INLINE
unsigned char
K_pte_ptr::page_order() const
{ return Kpdir::page_order_for_level(level); }


//-----------------------------------------------------------------------------
INTERFACE [arm && arm_v5]:

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

EXTENSION class K_pte_ptr :
  public Pte_v5_attribs<K_pte_ptr, Unsigned32>,
  public Pte_v_cache_no_asid<K_pte_ptr>
{};

//---------------------------------------------------------------------------
INTERFACE [arm && ((arm_v6plus && mp) || arm_v8)]:

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
INTERFACE [arm && (arm_v6 || arm_v7) && !mp]:

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
INTERFACE [arm && (arm_v5 || (arm_v6 && !arm_mpcore))]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0 }; };

//---------------------------------------------------------------------------
INTERFACE [arm && arm_mpcore]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0xa }; };

//---------------------------------------------------------------------------
INTERFACE [arm && ((mp && arm_v7) || arm_v8) && !arm_lpae]:

// S Sharable | RGN = Outer WB-WA | IRGN = Inner WB-WA | NOS
EXTENSION class Page
{ public: enum { Ttbr_bits = 0x6a }; };

//---------------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

EXTENSION class Page
{ public: enum { Ttbr_bits = 0 }; };

//---------------------------------------------------------------------------
INTERFACE [arm && !mp && arm_v7 && !arm_lpae]:
// armv7 w/o multiprocessing ext.

// RGN = Outer WB-WA | IRGN = Inner WB-WA
EXTENSION class Page
{ public: enum { Ttbr_bits = 0x09 }; };

//----------------------------------------------------------------------------
INTERFACE [arm && arm_v6 && !arm_mpcore]:

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
INTERFACE [arm && ((arm_v6 && arm_mpcore) || ((arm_v7 || arm_v8) && !arm_lpae))]:

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
INTERFACE [arm && arm_v6plus && !arm_lpae]:

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
    Section_no_cache   = 0x0402 | Section_shared | 0x10 /*XN*/,
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
INTERFACE [arm && arm_lpae && !cpu_virt]:

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
INTERFACE [arm && arm_lpae && cpu_virt]:

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

//---------------------------------------------------------------------------
INTERFACE [arm && (arm_v6 || (arm_v7 && !mp))]:

EXTENSION class K_pte_ptr : public Pte_cache_asid<K_pte_ptr> {};

//---------------------------------------------------------------------------
INTERFACE [arm && ((arm_v7 && mp) || arm_v8)]:

EXTENSION class K_pte_ptr : public Pte_no_cache_asid<K_pte_ptr> {};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) & 0x04) && ((error & 0x0d) == 0x001); }

//---------------------------------------------------------------------------
INTERFACE [arm && !arm_lpae && arm_v6plus]:

EXTENSION class K_pte_ptr :
  public Pte_v6plus_attribs<K_pte_ptr, Page>
{};

//---------------------------------------------------------------------------
INTERFACE [arm && arm_lpae && (arm_v7 || arm_v8) && cpu_virt]:

template<typename CLASS>
class Pte_ptr_t :
  public Pte_long_desc<CLASS>,
  public Pte_no_cache_asid<CLASS>,
  public Pte_stage2_attribs<CLASS, Page>,
  public Pte_generic<CLASS, Unsigned64>
{
public:
  Pte_ptr_t() = default;
  Pte_ptr_t(void *p, unsigned char level) : Pte_long_desc<CLASS>(p, level) {}
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_v6 || arm_v7 || arm_v8) && !arm_lpae]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) == 0x24) && ((error & 0x40f) == 0x001); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) == 0x24) && ((error & 0x3f) == 0x21); }

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
