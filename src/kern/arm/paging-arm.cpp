INTERFACE [arm && mmu]:

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

    Mword r = 0;
    if (attr.type == Page::Type::Normal())   r |= Page::CACHEABLE;
    if (attr.type == Page::Type::Buffered()) r |= Page::BUFFERED;
    if (attr.type == Page::Type::Uncached()) r |= Page::NONCACHEABLE;
    if (_this()->level == 0)
      return r | perms[cxx::int_value<Page::Rights>(attr.rights)];
    else
      {
        Mword p = perms[cxx::int_value<Page::Rights>(attr.rights)];
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

    Page::Rights rights;
    switch (r)
      {
      case 0x000: rights = Page::Rights::URX(); break;
      default:
      case 0x400: rights = Page::Rights::RWX(); break;
      case 0xc00: rights = Page::Rights::URWX(); break;
      }

    Page::Type type;
    switch (c)
      {
      default:
      case Page::CACHEABLE:    type = Page::Type::Normal(); break;
      case Page::BUFFERED:     type = Page::Type::Buffered(); break;
      case Page::NONCACHEABLE: type = Page::Type::Uncached(); break;
      }

    return Page::Attr(rights, type, Page::Kern::None(), Page::Flags::None());
  }

  bool attribs_compatible(Page::Attr attr) const
  {
    Page::Attr cur = attribs();

    if (cur.rights != attr.rights)
      return false;

    if (cur.type != attr.type)
      return false;

    return true;
  }

  Page::Flags access_flags() const
  { return Page::Flags::None(); }

  void del_rights(Page::Rights r)
  {
    if (!(r & Page::Rights::W()))
      return;

    auto p = access_once(_this()->pte);
    if ((p & 0xc00) == 0xc00)
      {
        p &= (_this()->level == 0) ? ~Mword{0xc00} : ~Mword{0xff0};
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
  Unsigned32 _attribs(Page::Attr attr) const
  {
    Mword lower = ATTRIBS::Mp_set_shared;
    if (attr.type == Page::Type::Normal())   lower |= ATTRIBS::CACHEABLE;
    if (attr.type == Page::Type::Buffered()) lower |= ATTRIBS::BUFFERED;
    if (attr.type == Page::Type::Uncached()) lower |= ATTRIBS::NONCACHEABLE;
    Mword upper = lower & ~0x0f;
    upper |= 0x10;  // AP[0]
    lower &= 0x0f;

    if (!(attr.kern & Page::Kern::Global()))
      upper |= 0x800;

    if (!(attr.rights & Page::Rights::W()))
      upper |= 0x200;

    if (attr.rights & Page::Rights::U())
      upper |= 0x20;

    if (!(attr.rights & Page::Rights::X()))
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
    auto c = access_once(_this()->pte);

    Page::Rights rights = Page::Rights::R();
    if (_this()->level == 0)
      {
        if (!(c & 0x10))
          rights |= Page::Rights::X();

        c = (c & 0x0f) | ((c >> 6) & 0xfff0);
      }
    else if (!(c & 0x01))
      rights |= Page::Rights::X();

    if (!(c & 0x200))
      rights |= Page::Rights::W();
    if (c & 0x20)
      rights |= Page::Rights::U();

    Page::Type type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE:    type = Page::Type::Normal(); break;
      case ATTRIBS::BUFFERED:     type = Page::Type::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = Page::Type::Uncached(); break;
      }

    Page::Kern k(0);
    if (!(c & 0x800))
      k |= Page::Kern::Global();

    return Page::Attr(rights, type, k, Page::Flags::None());
  }

  bool attribs_compatible(Page::Attr attr) const
  {
    Page::Attr cur = attribs();

    if (cur.rights != attr.rights)
      return false;

    if (cur.type != attr.type)
      return false;

    if (cur.kern != attr.kern)
      return false;

    return true;
  }

  Unsigned32 _page_bits() const { return 2; }

  Page::Flags access_flags() const
  { return Page::Flags::None(); }

  void del_rights(Page::Rights r)
  {
    Mword n_attr = 0;
    if (r & Page::Rights::W())
      {
        if (_this()->level == 0)
          n_attr = 0x200 << 6;
        else
          n_attr = 0x200;
      }

    if (r & Page::Rights::X())
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


//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae]:

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
  Unsigned64 _attribs(Page::Attr attr) const
  {
    Unsigned64 lower = 0x300; // inner sharable
    if (attr.type == Page::Type::Normal())
      lower |= ATTRIBS::CACHEABLE;
    if (attr.type == Page::Type::Buffered())
      lower |= ATTRIBS::BUFFERED;
    if (attr.type == Page::Type::Uncached())
      lower |= ATTRIBS::NONCACHEABLE;

    if (!(attr.kern & Page::Kern::Global()))
      lower |= 0x800;

    if (!(attr.rights & Page::Rights::W()))
      lower |= 0x080;

    if (attr.rights & Page::Rights::U())
      lower |= 0x040;

    if (ATTRIBS::Priv_levels == 1)
      lower |= 0x040; // the bit is RES1

    if (ATTRIBS::Priv_levels == 2 && !(attr.rights & Page::Rights::U()))
      {
        // Make kernel mappings never executable by userspace
        lower |= ATTRIBS::UXN;
      }

    if (!(attr.rights & Page::Rights::X()))
      lower |= ATTRIBS::UXN | ATTRIBS::PXN | ATTRIBS::XN;

    return lower;
  }

  Page::Attr attribs() const
  {
    auto c = access_once(_this()->pte);

    Page::Rights rights = Page::Rights::R();
    if (!(c & 0x80))
      rights |= Page::Rights::W();
    if (ATTRIBS::Priv_levels == 2)
      {
        if (c & 0x40)
          rights |= Page::Rights::U();
      }

    // Note that Page::UXN (if available) is dependent on Page::Rights::U()!
    if (!(c & (ATTRIBS::PXN | ATTRIBS::XN)))
      rights |= Page::Rights::X();

    Page::Type type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE:    type = Page::Type::Normal(); break;
      case ATTRIBS::BUFFERED:     type = Page::Type::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = Page::Type::Uncached(); break;
      }

    Page::Kern k(0);
    if (!(c & 0x800))
      k |= Page::Kern::Global();

    return Page::Attr(rights, type, k, Page::Flags::None());
  }

  bool attribs_compatible(Page::Attr attr) const
  {
    Page::Attr cur = attribs();

    if (cur.rights != attr.rights)
      return false;

    if (cur.type != attr.type)
      return false;

    if (cur.kern != attr.kern)
      return false;

    return true;
  }

  Unsigned64 _page_bits() const
  {
    return 0x400 | ((_this()->level == CLASS::max_level()) ? 3 : 1);
  }

  Page::Flags access_flags() const
  { return Page::Flags::None(); }

  void del_rights(Page::Rights r)
  {
    Unsigned64 n_attr = 0;
    if (r & Page::Rights::W())
      n_attr = 0x80;

    if (r & Page::Rights::X())
      n_attr |= ATTRIBS::UXN | ATTRIBS::PXN | ATTRIBS::XN;

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

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu]:


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
  Unsigned64 _attribs(Page::Attr attr) const
  {
    Unsigned64 lower = 0x300; // inner sharable
    if (attr.type == Page::Type::Normal())
      lower |= ATTRIBS::CACHEABLE;
    if (attr.type == Page::Type::Buffered())
      lower |= ATTRIBS::BUFFERED;
    if (attr.type == Page::Type::Uncached())
      lower |= ATTRIBS::NONCACHEABLE;

    // On AArch32 execution is only allowed if read access is permitted as well
    // On AArch64 this is not necessary, pages can be mapped execute-only
    if (attr.rights & Page::Rights::R()
        || (Proc::Is_32bit && attr.rights & Page::Rights::X()))
      lower |= (0x1 << 6);

    if (attr.rights & Page::Rights::W())
      lower |= (0x2 << 6);

    if (!(attr.rights & Page::Rights::X()))
      lower |= 0x0040000000000000;

    return lower;
  }

  Page::Attr attribs() const
  {
    auto c = access_once(_this()->pte);

    Page::Rights rights = Page::Rights::U();
    if (c & (0x1 << 6))
      rights |= Page::Rights::R();

    if (c & (0x2 << 6))
      rights |= Page::Rights::W();

    if (!(c & 0x0040000000000000))
      rights |= Page::Rights::X();

    Page::Type type;
    switch (c & ATTRIBS::Cache_mask)
      {
      default:
      case ATTRIBS::CACHEABLE:    type = Page::Type::Normal(); break;
      case ATTRIBS::BUFFERED:     type = Page::Type::Buffered(); break;
      case ATTRIBS::NONCACHEABLE: type = Page::Type::Uncached(); break;
      }

    return Page::Attr(rights, type, Page::Kern::None(), Page::Flags::None());
  }

  bool attribs_compatible(Page::Attr attr) const
  {
    Page::Attr cur = attribs();

    if (cur.rights != attr.rights)
      return false;

    if (cur.type != attr.type)
      return false;

    return true;
  }

  Unsigned64 _page_bits() const
  {
    return 0x400 | ((_this()->level == CLASS::max_level()) ? 3 : 1);
  }

  Page::Flags access_flags() const
  { return Page::Flags::None(); }

  void del_rights(Page::Rights r)
  {
    Unsigned64 n_attr = 0;
    Mword a_attr = 0;

    if (r & Page::Rights::R())
      a_attr |= 0x1 << 6;

    if (r & Page::Rights::W())
      a_attr |= 0x2 << 6;

    if (r & Page::Rights::X())
      n_attr |= 0x0040000000000000;

    if (!n_attr && !a_attr)
      return;

    auto p = access_once(_this()->pte);
    auto old = p;
    p |= n_attr;
    p &= ~Unsigned64{a_attr};

    if (p != old)
      write_now(_this()->pte, p);
  }
};


/**
 * Mixin for PTE pointers for 32bit page tables (short descriptors).
 */
template<typename CLASS>
class Pte_short_desc
{
public:
  static constexpr unsigned super_level() { return 0; }
  static constexpr unsigned max_level()   { return 1; }

  typedef Unsigned32 Entry;

  Unsigned32 *pte;
  unsigned char level;

  Pte_short_desc() = default;
  Pte_short_desc(void *p, unsigned char level)
  : pte(static_cast<Entry *>(p)), level(level)
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
    // 1 KiB second level tables
    return cxx::mask_lsb(access_once(pte), 10);
  }

  void set_next_level(Mword phys)
  {
    write_now(pte, phys | 1);
  }

  unsigned page_level() const
  { return level; }

  unsigned char page_order() const
  {
    if (level == 0)
      return 20; // 1 MiB
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
  : pte(static_cast<Unsigned64*>(p)), level(level)
  {}

  bool is_valid() const { return *pte & 1; }
  void clear() { write_now(pte, 0); }
  bool is_leaf() const
  {
    if (level >= CLASS::max_level())
      return true;
    return (*pte & 3) == 1;
  }

  Unsigned64 next_level() const
  {
    return cxx::get_lsb(cxx::mask_lsb(*pte, 12), 52);
  }

  void set_next_level(Unsigned64 phys)
  {
    // A new table was just allocated and cleared. Ensure the clearing is
    // observable to the MMU before the updated table descriptor. Otherwise the
    // next table walk might still see uninitialized PTEs.
    Mem::dmbst();
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

  Entry make_page(Phys_mem_addr addr, Page::Attr attr)
  {
    return _this()->_page_bits() | _this()->_attribs(attr)
           | cxx::int_value<Phys_mem_addr>(addr);
  }

  void set_page(Phys_mem_addr addr, Page::Attr attr)
  {
    set_page(make_page(addr, attr));
  }
};

//-------------------------------------------------------------------------------------
INTERFACE [arm && mmu && !arm_lpae]:

class K_pte_ptr :
  public Pte_short_desc<K_pte_ptr>,
  public Pte_generic<K_pte_ptr, Unsigned32>
{
public:
  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level)
  : Pte_short_desc(p, level) {}
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && !cpu_virt]:

// Kernel and user space are using stage-1 PTs and use the same page table
// attributes.
typedef Page Kernel_page_attr;

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && cpu_virt]:

// Fiasco is running with stage-1 PTs and the page attributes are an index into
// MAIR. OTOH user space is running on a stage-2 PT which stores the memory
// attributes directly (see class Page).
struct Kernel_page_attr
{
  enum Attribs_enum
  {
    Cache_mask    = 0x01c, ///< MAIR index 0..7
    NONCACHEABLE  = 0x000, ///< MAIR Attr0: Caching is off
    CACHEABLE     = 0x008, ///< MAIR Attr2: Cache is enabled
    BUFFERED      = 0x004, ///< MAIR Attr1: Write buffer enabled -- Normal, non-cached
  };

  enum
  {
    /// The EL2 translation regime supports one privilege level.
    Priv_levels = 1,
    // With only a single privilege level, there is no distinction between
    // privileged and unprivileged execute never.
    PXN = 0,          ///< Privileged Execute Never feature not available
    UXN = 0,          ///< Unprivileged Execute Never feature not available
    XN  = 1ULL << 54, ///< Execute Never
  };
};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae]:

class K_pte_ptr :
  public Pte_long_desc<K_pte_ptr>,
  public Pte_generic<K_pte_ptr, Unsigned64>,
  public Pte_long_attribs<K_pte_ptr, Kernel_page_attr>
{
public:
  K_pte_ptr() = default;
  K_pte_ptr(void *p, unsigned char level)
  : Pte_long_desc(p, level) {}
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_lpae]:

PUBLIC inline ALWAYS_INLINE
unsigned
K_pte_ptr::page_level() const
{ return level; }

PUBLIC inline ALWAYS_INLINE
unsigned char
K_pte_ptr::page_order() const
{ return Kpdir::page_order_for_level(level); }

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_v5]:

#include "types.h"

EXTENSION class K_pte_ptr :
  public Pte_v5_attribs<K_pte_ptr, Unsigned32>,
  public Pte_v_cache_no_asid<K_pte_ptr>
{};

//-----------------------------------------------------------------------------
INTERFACE [arm && mmu && !arm_lpae]:

#include "ptab_base.h"

typedef Ptab::List< Ptab::Traits< Unsigned32, 20, 12, true>,
                    Ptab::Traits< Unsigned32, 12, 8, true> > Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && (arm_v6 || (arm_v7 && !mp))]:

EXTENSION class K_pte_ptr : public Pte_cache_asid<K_pte_ptr> {};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && ((arm_v7 && mp) || arm_v8)]:

EXTENSION class K_pte_ptr : public Pte_no_cache_asid<K_pte_ptr> {};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_v5]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) & 0x04) && ((error & 0x0d) == 0x001); }

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !arm_lpae && arm_v6plus]:

EXTENSION class K_pte_ptr :
  public Pte_v6plus_attribs<K_pte_ptr, Page>
{};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae && (arm_v7 || arm_v8) && cpu_virt]:

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
IMPLEMENTATION [arm && (arm_v6 || arm_v7 || arm_v8) && !(arm_lpae || mpu)]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) == 0x24) && ((error & 0x40f) == 0x001); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_lpae || mpu)]:

PUBLIC static inline
Mword PF::is_alignment_error(Mword error)
{ return ((error >> 26) == 0x24) && ((error & 0x3f) == 0x21); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !(arm_lpae || mpu)]:

IMPLEMENT inline
Mword PF::is_translation_error(Mword error)
{
  return (error & 0x0d/*FSR_STATUS_MASK*/) == 0x05/*FSR_TRANSL*/;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_lpae || mpu)]:

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

//---------------------------------------------------------------------------
INTERFACE [arm && mpu]:

#include "mpu.h"

class Pdir : public Mpu_regions
{
public:
  Pdir() : Mpu_regions(Mpu_regions_mask{}) {}
  explicit Pdir(Mpu_regions_mask const &reserved) : Mpu_regions(reserved) {}
  explicit Pdir(Mpu_regions const &kpdir)
  : Mpu_regions(kpdir, Init::Reserved_regions)
  {}

  // retained from Pdir_t
  typedef Addr::Addr<0> Va;
  typedef Addr::Addr<0>::Diff_type Vs;
};

class Kpdir : public Pdir
{
public:
  /**
   * Index of fixed-function regions in Fiasco. They are referenced by the
   * entry assembly code too.
   */
  enum Well_known_regions {
    Kernel_text = 0,
    Kip = 1,
    Kernel_heap = 2,
  };

private:
  static Mpu_regions_mask well_known_regions()
  {
    Mpu_regions_mask m;
    m.set_bit(Kernel_text);
    m.set_bit(Kip);
    m.set_bit(Kernel_heap);
    return m;
  }

public:
  // All well known regions are reserved and need to be added explicitly.
  Kpdir() : Pdir(well_known_regions()) {}

  Address virt_to_phys(Address virt)
  { return virt; }
};
