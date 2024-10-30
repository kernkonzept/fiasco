INTERFACE [mips]:

#include "config.h"
#include "l4_msg_item.h"
#include "trap_state.h"

#include <cxx/cxx_int>

class Tlb_entry
{
public:
  enum : Mword
  {
    Global     = 0x001,
    Valid      = 0x002,
    Dirty      = 0x004,
    Write      = Dirty,
    Cache_mask = 0x038,
    Uncached   = 0x010,
    // uf UCA supported: C_UCA      = 0x038,
    C_UCA      = 0x010, // fallback to uncached
  };

  static Mword cached;
};

// ------------------------------------------------------------
INTERFACE [mips && 32bit]:

class Pdir_defs
{
public:
  enum : Mword
  {
    Used_levels = 2, // must be consistent with the used slots in PWSize
    Depth = 4,       // for JDB, it skips zero bit levels itself
    PT_level = 3,    // Level index for the final page table
    PWField_ptei = 6,
    PWField = (21 << 24) | (0 << 18) | (0 << 12) | (Config::PAGE_SHIFT        << 6) | PWField_ptei,
    PWSize  = (11 << 24) | (0 << 18) | (0 << 12) | ((21 - Config::PAGE_SHIFT) << 6) | 0,

    PWCtl_psn = 0
  };
};

// ------------------------------------------------------------
INTERFACE [mips && 64bit]:

class Pdir_defs
{
public:
  enum : Mword
  {
    Used_levels = 4, // must be consistent with the used slots in PWSize
    Depth = 4,       // for JDB, it skips zero bit levels itself
    PT_level = 3,    // Level index for the final page table
    PWField_ptei = 6,
    PWField = (39 << 24) | (30 << 18) | (21 << 12) | (Config::PAGE_SHIFT        << 6) | PWField_ptei,
    PWSize  = ( 9 << 24) | ( 9 << 18) | ( 9 << 12) | ((21 - Config::PAGE_SHIFT) << 6) | 0,

    PWCtl_psn = 0
  };

};

// ------------------------------------------------------------
INTERFACE [mips]:

class Pdir : public Pdir_defs
{
public:
  typedef Page_number Va;
  typedef Page_count  Vs;
  typedef Addr::Addr<Config::PAGE_SHIFT> Phys_addr;

  struct Walk_res
  {
    Mword *e;
    unsigned size;

  private:
    static Mword _rights(Page::Rights r)
    {
      Mword v = 0;

      if (r & Page::Rights::W())    v |= Write;
      if (!(r & Page::Rights::X())) v |= XI;
      if (!(r & Page::Rights::R())) v |= RI;

      return v;
    }

    static Mword _attr(Page::Attr attr)
    {
      Mword v = 0;

      if (attr.type == Page::Type::Normal())
        v = Tlb_entry::cached   << PWField_ptei;
      if (attr.type == Page::Type::Buffered())
        v = Tlb_entry::C_UCA    << PWField_ptei;
      if (attr.type == Page::Type::Uncached())
        v = Tlb_entry::Uncached << PWField_ptei;

      if (attr.kern & Page::Kern::Global())
        v |= Tlb_entry::Global  << PWField_ptei;

      v |= _rights(attr.rights);

      return v;
    }

  public:
    bool is_pte() const { return *e & Valid; }
    bool is_writable() const { return *e & Write; }
    Phys_addr page_addr() const
    {
      return Phys_addr((*e << (6 - PWField_ptei)) & (~0UL << 12));
    }

    Page::Rights rights() const
    {
      Page::Rights r = Page::Rights::U();

      if (!(*e & RI)) r |= Page::Rights::R();
      if (*e & Write) r |= Page::Rights::W();
      if (!(*e & XI)) r |= Page::Rights::X();

      return r;
    }

    Page::Attr attribs() const
    {
      Mword v = *e;
      Mword ct = (v >> PWField_ptei) & Tlb_entry::Cache_mask;

      Page::Rights r = rights();

      Page::Type t = Page::Type::Normal();
      if (ct == Tlb_entry::cached)
        t = Page::Type::Normal();
      else if (ct == Tlb_entry::Uncached)
        t = Page::Type::Uncached();
      else if (ct == Tlb_entry::C_UCA)
        t = Page::Type::Buffered();

      return Page::Attr(r, t, Page::Kern::None(), Page::Flags::None());
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
    {
      return Page::Flags::None();
    }

    static Mword make_page(Phys_addr pa, Page::Attr attr)
    {
      Mword v = cxx::int_value<Phys_addr>(pa);
      v >>= 6 - PWField_ptei;
      v |= _attr(attr) | Valid | Leaf;
      return v;
    }

    static Mword del_rights(Mword orig, Page::Rights del)
    {
      Mword r = _rights(del) ^ Rights_neg_mask;
      orig ^= Rights_neg_mask;
      orig &= ~r;
      orig ^= Rights_neg_mask;
      return orig;
    }

    void clear()
    {
      *e = Leaf;
    }
  };

  enum : Mword
  {
    Valid = Tlb_entry::Valid << PWField_ptei,
    XI    = 1UL << (PWField_ptei - 2),
    RI    = 2UL << (PWField_ptei - 2),
    Write = Tlb_entry::Write << PWField_ptei,
    Leaf  = 1UL << PWCtl_psn,

    Rights_mask     = Write | XI | RI,
    Rights_neg_mask = XI | RI,

    Entries = 1UL << ((PWSize >> 24) & 0x3f),
  };

  static constexpr unsigned l_size(unsigned lvl)
  { return (PWSize >> ((4 - lvl) * 6)) & 0x3f; }

  static constexpr unsigned l_field(unsigned lvl)
  { return (PWField >> ((4 - lvl) * 6)) & 0x3f; }

  Mword _entries[Entries];

  // for JDB
  struct Pte_ptr
  {
    typedef Mword Entry;
    Mword *e;
    unsigned char level;
    Pte_ptr() = default;
    Pte_ptr(void *p, unsigned char lvl)
    : e(static_cast<Mword *>(p)), level(lvl) {}
    bool is_leaf() const  { return *e & Leaf; }
    bool is_valid() const { return !(*e & Leaf) || (*e & Valid); }
    Address next_level() const { return *e; }
    Address page_addr() const
    { return (*e << (6 - PWField_ptei)) & (~0UL << 12); }
  };

  struct Levels
  {
    static constexpr unsigned entry_size(unsigned)
    { return sizeof(Mword); }

    static constexpr unsigned long length(unsigned l)
    { return 1UL << l_size(l); }
  };

  static constexpr unsigned lsb_for_level(unsigned l)
  { /* Va relative */ return l_field(l) - l_field(PT_level); }
  // end JDB
};

class Kpdir : public Pdir {};

//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include "trap_state.h"

Mword Tlb_entry::cached;

IMPLEMENT inline NEEDS["trap_state.h"]
Mword
PF::is_usermode_error(Mword error)
{ return (error & Trap_state::C_src_context_mask) == Trap_state::C_src_user; }

IMPLEMENT inline
Mword
PF::is_read_error(Mword cause)
{
  // bit 0 in the exception code denotes a write / store access
  // in all TLB, Address, and bus errors
  // 0x13 is triggered on a read access if the RI (read inhibit) bit is set.
  return !(cause & 4) || ((cause & 0x7c) == (0x13 << 2));
}

PUBLIC static inline NEEDS["trap_state.h"]
Mword
PF::is_read_error(Trap_state::Cause const cause)
{ return is_read_error(cause.raw); }

PUBLIC static inline
Mword
PF::is_xi_error(Mword cause)
{
  // TLBXI
  auto code = cause & 0x7c;
  return code == (0x14 << 2);
}

PUBLIC static inline
Mword
PF::is_tlb_rights_error(Mword cause)
{
  auto code = cause & 0x7c;
  return code == (0x14 << 2) || code == (0x13 << 2);
}

PUBLIC static inline NEEDS["trap_state.h"]
Mword
PF::is_translation_error(Trap_state::Cause const cause)
{
  return    cause.exc_code() == 2  // TLBL
         || cause.exc_code() == 3; // TLBS
}

IMPLEMENT inline NEEDS["trap_state.h"]
Mword
PF::is_translation_error(Mword cause)
{ return is_translation_error(Trap_state::Cause(cause)); }

IMPLEMENT inline NEEDS[PF::is_read_error, PF::is_xi_error]
Mword PF::addr_to_msgword0(Address pfa, Mword cause)
{
  Mword a = pfa & ~7;
  if (is_translation_error(cause))
    a |= 1; // NOT present

  if(!is_read_error(cause))
    a |= 2;

  if (is_xi_error(cause))
    {
      // Executing non-executable page.
      a |= 4;
    }

  return a;
}

PUBLIC
void
Pdir::clear()
{
  for (auto &e: _entries)
    e = Leaf;
}

PUBLIC
Address
Pdir::virt_to_phys(Address virt) const
{
  Mword const *p = _entries;
  Mword v = 0;
  Mword field;

  for (unsigned lvl = 0; lvl < 4; ++lvl)
    {
      auto const size = l_size(lvl);
      if (!size)
        continue;

      field = l_field(lvl);
      Mword const mask = (1UL << size) - 1;
      auto idx = (virt >> field) & mask;
      v = p[idx];
      if (v & Leaf)
        break; // this was the last level

      p = reinterpret_cast<Mword const *>(v);
    }

  if (!(v & Valid))
    return ~0UL;

  v = (v << (6 - PWField_ptei)) & (~0UL << 12);
  v |= virt & ((1UL << field) - 1);
  return v;
}

PUBLIC template<typename ALLOC = Ptab::Null_alloc> inline
Pdir::Walk_res
Pdir::walk(Va _virt, unsigned isize = 0,
           ALLOC const &alloc = ALLOC())
{
  Mword virt = cxx::int_value<Virt_addr>(_virt);
  Mword *p = _entries;
  Walk_res r;
  r.size = 32; // full space at base ptr level

  bool do_alloc = false;

  for (unsigned lvl = 0; lvl < 4; ++lvl)
    {
      auto const size = l_size(lvl);
      if (!size)
        continue;

      if (do_alloc)
        {
          p = static_cast<Mword*>(alloc.alloc(Bytes(sizeof(Mword) << size)));
          if (!p)
            return r; // OOM

          for (unsigned i = 0; i < (1UL << size); ++i)
            p[i] = Leaf;

          *r.e = reinterpret_cast<Mword>(p);
        }

      r.size = l_field(lvl);
      Mword const mask = (1UL << size) - 1;
      auto idx = (virt >> r.size) & mask;
      r.e = &p[idx];
      if (*r.e & Leaf)
        {
          if (r.size == isize || r.is_pte())
            return r;

          if (!alloc.valid())
            return r;

          do_alloc = true;
          continue;
        }
      p = reinterpret_cast<Mword*>(*r.e);
    }

  return r;
}

PUBLIC template<typename ALLOC>
void
Pdir::destroy(Va _start, Va _end, ALLOC const &alloc)
{
  // nothing to free for a single level array
  if (Used_levels < 2)
    return;

  // 1. this code supports level 0 aligned granularity only
  // 2. Assume that the first entry in PWSize is valid not 0

  Mword start = cxx::int_value<Virt_addr>(_start);
  Mword end   = cxx::int_value<Virt_addr>(_end);

  Mword l_mask[Used_levels];
  Mword l_shift[Used_levels];
  Mword l_bits[Used_levels];
  Mword *p[Used_levels - 1];

  unsigned n = 0;
  for (unsigned l = 0; l < 4; ++l)
    {
      auto const s = l_size(l);
      if (!s)
        continue;

      l_bits[n] = s;
      l_mask[n] = (1UL << s) - 1;
      l_shift[n] = l_field(l);
      ++n;
    }

  n = 0;
  p[0] = _entries;
  while (start <= end)
    {
      auto idx = (start >> l_shift[n]) & l_mask[n];
      Mword v = p[n][idx];
      if (!(v & Leaf))
        {
          if (n < Used_levels - 2)
            {
              ++n;
              p[n] = reinterpret_cast<Mword *>(v);
              continue;
            }

          alloc.free(reinterpret_cast<void *>(v),
                     Bytes(sizeof(Mword) << l_bits[n + 1]));
        }

      if (start >= end)
        break;

      start += (1UL << l_shift[n]);
      while (n && ((start >> l_shift[n]) & l_mask[n]) == 0)
        {
          alloc.free(reinterpret_cast<void *>(p[n]),
                     Bytes(sizeof(Mword) << l_bits[n]));
          --n;
        }
    }

  while (n)
    {
      alloc.free(reinterpret_cast<void *>(p[n]),
                 Bytes(sizeof(Mword) << l_bits[n]));
      --n;
    }
}

