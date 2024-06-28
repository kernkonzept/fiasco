INTERFACE [ia32 || amd64 || ux]:

#include "types.h"
#include "config.h"


EXTENSION class Page
{
public:
  enum Attribs_enum
  {
    MAX_ATTRIBS   = 0x00000006,
    Cache_mask    = 0x00000018, ///< Cache attribute mask
    CACHEABLE     = 0x00000000, ///< PAT=0, PCD=0, PWT=0: PAT0 (WB)
    BUFFERED      = 0x00000010, ///< PAT=0, PCD=1, PWT=0: PAT2 (WC)
    NONCACHEABLE  = 0x00000018, ///< PAT=0, PCD=1, PWT=1: PAT3 (UC)
  };
};

EXTENSION class Pt_entry
{
private:
  static Unsigned32 _cpu_global;
  static unsigned _super_level;
  static bool _have_superpages;
};

EXTENSION class Pte_ptr : private Pt_entry
{
public:
  using Pt_entry::Super_level;
};


//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64 || ux]:

#include "atomic.h"

bool Pt_entry::_have_superpages;
unsigned  Pt_entry::_super_level;

PUBLIC static inline
void
Pt_entry::have_superpages(bool yes)
{
  _have_superpages = yes;
  _super_level = yes ? Super_level : (Super_level + 1);
}

PUBLIC static inline
unsigned
Pt_entry::super_level()
{ return _super_level; }


PUBLIC inline
bool
Pte_ptr::is_valid() const
{ return *pte & Valid; }

PUBLIC inline
bool
Pte_ptr::is_leaf() const
{ return level == Pdir::Depth || (*pte & Pse_bit); }

/**
 * \pre is_leaf() == false
 */
PUBLIC inline
Mword
Pte_ptr::next_level() const
{ return cxx::mask_lsb(*pte, static_cast<unsigned>(Config::PAGE_SHIFT)); }

/**
 * \pre cxx::get_lsb(phys_addr, Config::PAGE_SHIFT) == 0
 */
PUBLIC inline
void
Pte_ptr::set_next_level(Mword phys_addr)
{ *pte = phys_addr | Valid | User | Writable; }

PUBLIC inline
void
Pte_ptr::set_page(Mword phys, Mword attr)
{
  Mword v = phys | Valid | attr;
  if (level < Pdir::Depth)
    v |= Pse_bit;
  *pte = v;
}

PUBLIC inline
Pte_ptr const &
Pte_ptr::operator ++ ()
{
  ++pte;
  return *this;
}

PUBLIC inline
Mword
Pte_ptr::page_addr() const
{ return cxx::mask_lsb(*pte, Pdir::page_order_for_level(level)) & ~Mword{XD}; }

PUBLIC static inline ALWAYS_INLINE
Mword
Pte_ptr::make_flags(Page::Flags flags)
{
  Mword r = 0;

  if (flags & Page::Flags::Referenced()) r |= Referenced;
  if (flags & Page::Flags::Dirty()) r |= Dirty;

  return r;
}

PUBLIC static inline ALWAYS_INLINE
Mword
Pte_ptr::make_attribs(Page::Attr attr)
{
  Mword r = make_flags(attr.flags);

  if (attr.rights & Page::Rights::W()) r |= Writable;
  if (attr.rights & Page::Rights::U()) r |= User;
  if (!(attr.rights & Page::Rights::X())) r |= XD;

  if (attr.type == Page::Type::Normal()) r |= Page::CACHEABLE;
  if (attr.type == Page::Type::Buffered()) r |= Page::BUFFERED;
  if (attr.type == Page::Type::Uncached()) r |= Page::NONCACHEABLE;

  if (attr.kern & Page::Kern::Global()) r |= global();

  return r;
}

PUBLIC inline
void
Pte_ptr::set_attribs(Page::Attr attr)
{
  *pte = (*pte & ~(ATTRIBS_MASK | Page::Cache_mask)) | make_attribs(attr);
}

PUBLIC inline
Mword
Pte_ptr::make_page(Phys_mem_addr addr, Page::Attr attr)
{
  Mword r = (level < Pdir::Depth) ? Mword{Pse_bit} : 0;
  r |= make_attribs(attr);

  return cxx::int_value<Phys_mem_addr>(addr) | r | Valid;
}

PUBLIC inline
void
Pte_ptr::set_page(Mword p)
{
  write_now(pte, p);
}

PUBLIC inline
Page::Attr
Pte_ptr::attribs() const
{
  Mword raw = *pte;

  Page::Rights r = Page::Rights::R();
  if (raw & Writable) r |= Page::Rights::W();
  if (raw & User) r |= Page::Rights::U();
  if (!(raw & XD)) r |= Page::Rights::X();

  Page::Type t;
  switch (raw & Page::Cache_mask)
    {
    default:
    case Page::CACHEABLE:    t = Page::Type::Normal(); break;
    case Page::BUFFERED:     t = Page::Type::Buffered(); break;
    case Page::NONCACHEABLE: t = Page::Type::Uncached(); break;
    }

  Page::Kern k = Page::Kern::None();
  if (raw & global()) k |= Page::Kern::Global();

  Page::Flags f = Page::Flags::None();
  if (raw & Referenced) f |= Page::Flags::Referenced();
  if (raw & Dirty) f |= Page::Flags::Dirty();

  return Page::Attr(r, t, k, f);
}

PUBLIC inline
unsigned char
Pte_ptr::page_order() const
{ return Pdir::page_order_for_level(level); }

PUBLIC inline NEEDS["atomic.h"]
Page::Flags
Pte_ptr::access_flags()
{
  if (!is_valid())
    return Page::Flags::None();

  Page::Flags f = Page::Flags::None();
  for (;;)
    {
      Mword raw = *pte;

      if (raw & Referenced) f |= Page::Flags::Referenced();
      if (raw & Dirty) f |= Page::Flags::Dirty();

      if (f == Page::Flags::None())
        return f;

      if (cas(pte, raw, raw & ~(Dirty | Referenced)))
        return f;
    }
}

PUBLIC inline
void
Pte_ptr::clear()
{ *pte = 0; }

PUBLIC static inline
void
Pte_ptr::write_back(void *, void *)
{}

PUBLIC static inline
void
Pte_ptr::write_back_if(bool)
{}

PUBLIC inline
void
Pte_ptr::add_attribs(Page::Attr attr)
{ *pte |= make_attribs(attr); }

PUBLIC inline
void
Pte_ptr::del_attribs(Page::Attr attr)
{ *pte &= ~make_attribs(attr); }

PUBLIC inline
void
Pte_ptr::add_flags(Page::Flags flags)
{ *pte |= make_flags(flags); }

PUBLIC inline
void
Pte_ptr::del_rights(Page::Rights r)
{
  if (r & Page::Rights::W())
    *pte &= ~Writable;

  if (r & Page::Rights::X())
    *pte |= XD;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64 || ux) && kernel_isolation]:

PUBLIC static inline
void
Pt_entry::enable_global()
{}

/**
 * Global entries are entries that are not automatically flushed when the
 * page-table base register is reloaded. They are intended for kernel data
 * that is shared between all tasks.
 * @return global page-table--entry flags
 */
PUBLIC static inline
Unsigned32
Pt_entry::global()
{ return 0; }

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64 || ux) && !kernel_isolation]:

Unsigned32 Pt_entry::_cpu_global = Pt_entry::L4_global;

PUBLIC static inline
void
Pt_entry::enable_global()
{ _cpu_global |= Cpu_global; }

/**
 * Global entries are entries that are not automatically flushed when the
 * page-table base register is reloaded. They are intended for kernel data
 * that is shared between all tasks.
 * @return global page-table--entry flags
 */
PUBLIC static inline
Unsigned32
Pt_entry::global()
{ return _cpu_global; }


//--------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64 || ux]:
#include "cpu.h"
#include "mem_layout.h"
#include "regdefs.h"

IMPLEMENT inline NEEDS["regdefs.h"]
Mword PF::is_translation_error(Mword error)
{
  return !(error & PF_ERR_PRESENT);
}

IMPLEMENT inline NEEDS["regdefs.h"]
Mword PF::is_usermode_error(Mword error)
{
  return (error & PF_ERR_USERMODE);
}

IMPLEMENT inline NEEDS["regdefs.h"]
Mword PF::is_read_error(Mword error)
{
  return !(error & PF_ERR_WRITE);
}

IMPLEMENT inline NEEDS["regdefs.h"]
Mword PF::addr_to_msgword0(Address pfa, Mword error)
{
  Mword v = (pfa & ~0x7) | (error & (PF_ERR_PRESENT | PF_ERR_WRITE));
  if (error & PF_ERR_INSTFETCH)
    v |= 0x4;
  return v;
}

