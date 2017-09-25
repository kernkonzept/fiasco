INTERFACE [ia32 || amd64 || ux]:

#include "types.h"
#include "config.h"


EXTENSION class Page
{
public:
  enum Attribs_enum
  {
    MAX_ATTRIBS   = 0x00000006,
    Cache_mask    = 0x00000018, ///< Cache attrbute mask
    CACHEABLE     = 0x00000000,
    BUFFERED      = 0x00000010,
    NONCACHEABLE  = 0x00000018,
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
{ return cxx::mask_lsb(*pte, (unsigned)Config::PAGE_SHIFT); }

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
{ return cxx::mask_lsb(*pte, Pdir::page_order_for_level(level)) & ~Mword(XD); }


PUBLIC inline
void
Pte_ptr::set_attribs(Page::Attr attr)
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;
  Mword r = 0;
  if (attr.rights & R::W()) r |= Writable;
  if (attr.rights & R::U()) r |= User;
  if (!(attr.rights & R::X())) r |= XD;
  if (attr.type == T::Normal()) r |= Page::CACHEABLE;
  if (attr.type == T::Buffered()) r |= Page::BUFFERED;
  if (attr.type == T::Uncached()) r |= Page::NONCACHEABLE;
  if (attr.kern & K::Global()) r |= global();
  *pte = (*pte & ~(ATTRIBS_MASK | Page::Cache_mask)) | r;
}

PUBLIC inline
void
Pte_ptr::create_page(Phys_mem_addr addr, Page::Attr attr)
{
  Mword r = (level < Pdir::Depth) ? (Mword)Pse_bit : 0;
  typedef L4_fpage::Rights R;
  typedef Page::Type T;
  typedef Page::Kern K;
  if (attr.rights & R::W()) r |= Writable;
  if (attr.rights & R::U()) r |= User;
  if (!(attr.rights & R::X())) r |= XD;
  if (attr.type == T::Normal()) r |= Page::CACHEABLE;
  if (attr.type == T::Buffered()) r |= Page::BUFFERED;
  if (attr.type == T::Uncached()) r |= Page::NONCACHEABLE;
  if (attr.kern & K::Global()) r |= global();
  *pte = cxx::int_value<Phys_mem_addr>(addr) | r | Valid;
}

PUBLIC inline
Page::Attr
Pte_ptr::attribs() const
{
  typedef L4_fpage::Rights R;
  typedef Page::Type T;

  Mword _raw = *pte;
  R r = R::R();
  if (_raw & Writable) r |= R::W();
  if (_raw & User) r |= R::U();
  if (!(_raw & XD)) r |= R::X();

  T t;
  switch (_raw & Page::Cache_mask)
    {
    default:
    case Page::CACHEABLE:    t = T::Normal(); break;
    case Page::BUFFERED:     t = T::Buffered(); break;
    case Page::NONCACHEABLE: t = T::Uncached(); break;
    }
  // do not care for kernel special flags, as this is used for user
  // level mappings
  return Page::Attr(r, t);
}

PUBLIC inline
bool
Pte_ptr::add_attribs(Page::Attr attr)
{
  typedef L4_fpage::Rights R;
  unsigned long a = 0;

  if (attr.rights & R::W())
    a = Writable;

  if (attr.rights & R::X())
    a |= XD;

  if (!a)
    return false;

  auto p = access_once(pte);
  auto o = p;
  p ^= XD;
  p |= a;
  p ^= XD;

  if (o != p)
    {
      write_now(pte, p);
      return true;
    }
  return false;
}

PUBLIC inline
void
Pte_ptr::add_attribs(Mword attr)
{ *pte |= attr; }

PUBLIC inline
unsigned char
Pte_ptr::page_order() const
{ return Pdir::page_order_for_level(level); }

PUBLIC inline NEEDS["atomic.h"]
L4_fpage::Rights
Pte_ptr::access_flags() const
{

  if (!is_valid())
    return L4_fpage::Rights(0);

  L4_fpage::Rights r;
  for (;;)
    {
      auto raw = *pte;

      if (raw & Dirty)
        r = L4_fpage::Rights::RW();
      else if (raw & Referenced)
        r = L4_fpage::Rights::R();
      else
        return L4_fpage::Rights(0);

      if (mp_cas(pte, raw, raw & ~(Dirty | Referenced)))
        return r;
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
Pte_ptr::del_attribs(Mword attr)
{ *pte &= ~attr; }

PUBLIC inline
void
Pte_ptr::del_rights(L4_fpage::Rights r)
{
  if (r & L4_fpage::Rights::W())
    *pte &= ~Writable;

  if (r & L4_fpage::Rights::X())
    *pte |= XD;
}

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
  Mword v = (pfa & ~0x7) | (error &  (PF_ERR_PRESENT | PF_ERR_WRITE));
  if (error & (1 << 4)) v |= 0x4;
  return v;
}

