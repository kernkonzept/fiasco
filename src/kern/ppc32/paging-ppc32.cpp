INTERFACE[ppc32]:

#include "types.h"

class PF {};
class Page {};

//------------------------------------------------------------------------------
INTERFACE[ppc32]:

#include <cassert>
#include "types.h"
#include "ptab_base.h"

class Paging {};

class Pte_ptr
{
public:
  enum
  {
    Super_level   = 0,
    Htab_entry    = 0x00000400,  ///< is raw entry of htab
    Valid         = 0x00000004,  ///< Valid
    Pse_bit       = 0x00000800,  ///< Indicates a super page in hierarch. pgt.
    Writable      = 0x00000002,  ///< Writable
    User          = 0x00000001,  ///< User accessible
    Write_through = 0x00000050,  ///< Write through
    Cacheable     = 0x00000000,  ///< Cache is enabled
    Cacheable_mask= 0xffffff87,
    Noncacheable  = 0x00000020,  ///< Caching is off
    Referenced    = 0x00000100,  ///< Page was referenced
    Dirty         = 0x00000080,  ///< Page was modified
    Pfn           = 0xfffff000,  ///< page frame number
//    Cpu_global    = 0x00000100, ///< pinned in the TLB
//    L4_global     = 0x00000200, ///< pinned in the TLB
  };

  typedef Mword Entry;

  Pte_ptr() = default;
  Pte_ptr(void *p, unsigned char level) : pte((Mword*)p), level(level) {}

  bool is_valid() const { return *pte & Valid; }
  void clear() { *pte = 0; }
  bool is_leaf() const
  {
    return !is_super_page(); // CHECK!
  }

  Mword next_level() const
  {
    return 0; // FIX
  }

  void set_next_level(Mword phys)
  {
    (void)phys; // FIX
  }

  Mword addr() const { return *pte & Pfn;}
  bool is_super_page() const { return *pte & Pse_bit; }
  Mword raw() const { return *pte; }

protected:
  Entry *pte;
  unsigned char level;
};

class Pt_entry : public Pte_ptr
{
public:
  Mword leaf() const { return true; }
  void set(Address p, bool intermed, bool /*present*/, unsigned long attrs = 0)
  {
    *pte = (p & Pfn)
      | (intermed ? (Writable | User) : 0) | attrs;
    *pte &= intermed ? (Mword)Cacheable_mask : ~0;
  }
};

class Pd_entry : public Pte_ptr
{
public:
  Mword leaf() const { return false; }
  void set(Address p, bool intermed, bool present, unsigned long attrs = 0)
  {
    *pte = (p & Pfn) | (present ? (Mword)Valid : 0)
      | (intermed ? (Writable | User) : Pse_bit) | attrs;
    *pte &= intermed ? (Mword)Cacheable_mask : ~0;
  }
};

class Pte_htab
{
public:
  Pte_htab(Mword, Mword, Mword);

  union {
    struct {
      Mword valid : 1; // valid bit
      Mword vsid  :24; // address-space id
      Mword h     : 1; // hash-function bit
      Mword api   : 6; // abbreviated-page index
      Mword rpn   :20; // physical-page numer
      Mword zero  : 3; // reserved
      Mword r     : 1; // referenced bit
      Mword c     : 1; // changed bit
      Mword wimg  : 4; // cache controls
      Mword zero1 : 1; // reserved
      Mword pp    : 2; // protection bits
    } pte;
    struct {
      Unsigned32 raw0;
      Unsigned32 raw1;
    } raw;
  };

  bool inline valid()
  { return this->pte.valid; }

  bool inline v_equal(Pte_htab *entry)
  { return this->raw.raw0 == entry->raw.raw0; }

  bool inline p_equal(Pte_htab *entry)
  { return this->raw.raw1 == entry->raw.raw1; }

  Address inline virt()
  { return this->raw.raw0; }

  Address inline phys()
  { return this->raw.raw1; }
};

EXTENSION class Page
{
public:
  typedef Unsigned32 Attribs;
  enum Attribs_enum
  {
    Cache_mask   = 0x00000078,
    CACHEABLE    = 0x00000000,
    NONCACHEABLE = 0x00000040,
    BUFFERED     = 0x00000080, //XXX not sure
  };
};


typedef Ptab::List< Ptab::Traits<Unsigned32, 22, 10, true>,
                    Ptab::Traits<Unsigned32, 12, 10, true> > Ptab_traits;

typedef Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List Ptab_traits_vpn;
typedef Ptab::Page_addr_wrap<Page_number, Virt_addr::Shift> Ptab_va_vpn;


IMPLEMENTATION[ppc32]:

#include "config.h"
#include "msr.h"
#include "lock_guard.h"
#include "cpu_lock.h"


/* this functions do nothing on PPC32 architecture */
PUBLIC static inline
Address
Paging::canonize(Address addr)
{
  return addr;
}

PUBLIC static inline
Address
Paging::decanonize(Address addr)
{
  return addr;
}

//---------------------------------------------------------------------------
IMPLEMENT inline 
Mword PF::is_translation_error(Mword error)
{
  return !(error & 1 << 30) /* DSISR/SRR1 bit 1 */;
}

IMPLEMENT inline NEEDS["msr.h"]
Mword PF::is_usermode_error(Mword error)
{
  return (error & Msr::Msr_pr);
}

IMPLEMENT inline
Mword PF::is_read_error(Mword error)
{
  return !(error & (1 << 25)) /* DSISR bit 6*/;
}

IMPLEMENT inline
Mword PF::addr_to_msgword0(Address pfa, Mword error)
{
  Mword a = pfa & ~3;
  if(is_translation_error(error))
    a |= 1;
  if(!is_read_error(error))
    a |= 2;
  return a;
}

//---------------------------------------------------------------------------

#if 0
PUBLIC inline
Pte_base &
Pte_base::operator = (Pte_base const &other)
{
  *pte = other.raw();
  return *this;
}

PUBLIC inline
Pte_base &
Pte_base::operator = (Mword raw)
{
  *pte = raw;
  return *this;
}

PUBLIC inline
Mword
Pte_base::raw() const
{
  return *pte;
}
#endif

PUBLIC inline
void
Pte_ptr::add_attr(Page::Attr attr)
{
  (void)attr;
  // FIX
  //*pte |= attr;
}

//PUBLIC inline
//void
//Pte_base::del_attr(Mword attr)
//{
//*pte &= ~attr;
//}

#if 0
PUBLIC inline
int
Pte_base::writable() const
{
  return *pte & Writable;
}
#endif

PUBLIC inline
bool
Pte_ptr::is_htab_entry() const
{
  return ((*pte & Htab_entry) && !(*pte & Valid));
}

PUBLIC inline
unsigned
Pte_ptr::to_htab_entry(unsigned page_attribs = 0)
{
  *pte |= Htab_entry;
  *pte &= ~Valid;
  return page_attribs & ~Valid;
}

PUBLIC inline
bool
Pte_ptr::is_htab_ptr() const
{
  return (*pte & Valid);
}

PUBLIC inline
void
Pte_ptr::to_htab_ptr()
{
  *pte |= Valid;
}

//PUBLIC inline
//Address
//Pt_entry::pfn() const
//{
  //return *pte & Pfn;
//}

//------------------------------------------------------------------------------
/*
 * Hash Page-table entries
 */
IMPLEMENT inline NEEDS[Pte_htab::api]
Pte_htab::Pte_htab(Address pa, Address ea, Address vsid)
{
  this->raw.raw0      = 0;
  this->pte.valid     = 1;
  this->pte.vsid      = vsid;
  this->pte.api       = api(ea);
  this->raw.raw1      = pa;
}

PRIVATE static inline
Address
Pte_htab::api(Address ea)
{
  //bits 4-9
  return (ea >> 22) & 0x3f;
}

PRIVATE inline
Address
Pte_htab::api_reverse()
{
  return this->pte.api << 22;
}

PUBLIC inline NEEDS[Pte_htab::api_reverse, "config.h"]
Address
Pte_htab::pte_to_ea()
{
  Address pteg = (Address)this;
  //if secondary hash
  if(this->pte.h)
    pteg = ~pteg;

  Address va = 0x3ff /*10bit*/ & (this->pte.vsid ^ (pteg >> 6));
  va <<= Config::PAGE_SHIFT;
  va |= this->api_reverse();
  va |= (this->pte.vsid & 0xf) << 28;
  return va;
}


PUBLIC static inline
Pte_htab *
Pte_htab::addr_to_pte(Address pte_addr)
{
  return reinterpret_cast<Pte_htab*>(pte_addr & ~Pte_ptr::Valid);
}

PUBLIC static inline
Address
Pte_htab::pte_to_addr(Pte_ptr *e)
{
  Address raw;

  if (e->is_htab_entry())
    raw = e->raw();
  else
    {
      Pte_htab *pte_phys = addr_to_pte(e->raw());
      raw = pte_phys->phys();
    }

  return raw;
}

PUBLIC static
bool
Pte_htab::pte_lookup(Pte_ptr *e, Address *phys = 0,
                     unsigned *page_attribs = 0)
{
  auto guard = lock_guard(cpu_lock);

  Address raw;

  if(e->is_htab_entry())
    {
      raw = e->raw();
    }
  else
    {
      Pte_htab *pte = addr_to_pte(e->raw());
      raw = pte->phys();

      if(!pte->valid())
        return false;
    }

  assert(raw);

  if(phys) *phys = raw & (~0UL << Config::PAGE_SHIFT);
  if(page_attribs) *page_attribs = raw;

  return true;
}

PUBLIC static inline
bool
Pte_ptr::need_cache_write_back(bool current_pt)
{ return current_pt; }

PUBLIC inline// NEEDS["mem_unit.h"]
void
Pte_ptr::write_back_if(bool current_pt, Mword /*asid*/ = 0)
{
  (void)current_pt;
  //  if (current_pt)
  //        Mem_unit::clean_dcache(pte);
}

PUBLIC static inline// NEEDS["mem_unit.h"]
void
Pte_ptr::write_back(void *start, void *end)
{ (void)start; (void)end; }

