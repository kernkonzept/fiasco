INTERFACE:

#include "l4_types.h"

class X86desc
{
public:
  enum
  {
    Accessed            = 0x01,
    Access_kernel       = 0x00,
    Access_user         = 0x60,
    Access_present      = 0x80,

    Access_tss          = 0x09,
    Access_intr_gate    = 0x0e,
    Access_trap_gate    = 0x0f,

    Long_mode           = 0x02, // XXX for code segments
  };
};


class Gdt_entry : public X86desc
{
public:
  enum
  {
    Access_type_user    = 0x10,
    Access_code_read    = 0x1a,
    Access_data_write   = 0x12,
    Size_32             = 0x04,
  };

  union {
    struct {
      Unsigned16   limit_low;
      Unsigned16   base_low;
      Unsigned8    base_med;
      Unsigned8    access;
      Unsigned8    limit_high;
      Unsigned8    base_high;
    };
    Unsigned64 raw;
  };

} __attribute__((packed));


class Idt_entry : public X86desc
{
private:
  Unsigned16 _offset_low;
  Unsigned16 _selector;
  Unsigned8  _ist;
  Unsigned8  _access;
  Unsigned16 _offset_high;
} __attribute__((packed));



class Pseudo_descriptor
{
  Unsigned16 _limit;
  Mword _base;
} __attribute__((packed));



//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 | ux]:

PUBLIC inline
Address Idt_entry::offset() const
{ return (Address)_offset_low | ((Address)_offset_high << 16); }

PUBLIC inline
Idt_entry::Idt_entry()
{}

// interrupt gate
PUBLIC inline
Idt_entry::Idt_entry(Address entry, Unsigned16 selector, Unsigned8 access, unsigned = 0)
{
  _offset_low  = entry & 0x0000ffff;
  _selector    = selector;
  _ist         = 0;
  _access      = access | X86desc::Access_present;
  _offset_high = (entry & 0xffff0000) >> 16;
}

// task gate
PUBLIC inline
Idt_entry::Idt_entry(Unsigned16 selector, Unsigned8 access)
{
  _offset_low  = 0;
  _selector    = selector;
  _ist         = 0;
  _access      = access | X86desc::Access_present;
  _offset_high = 0;
}

PUBLIC static inline
Idt_entry
Idt_entry::free(Unsigned16 val)
{
  Idt_entry e;
  e._access = 0;
  e._offset_low = val;
  return e;
}

PUBLIC inline
Unsigned16
Idt_entry::get_free_val() const
{ return _offset_low; }

PUBLIC inline
Address
Gdt_entry::base() const
{
  return (Address)base_low | ((Address)base_med  << 16)
         | ((Address)base_high << 24);
}

//----------------------------------------------------------------------------
INTERFACE [amd64]:
EXTENSION
class Idt_entry
{
private:
  Unsigned32   _offset_high1;
  Unsigned32   _ignored;
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

#include <panic.h>

PUBLIC inline
Address Idt_entry::offset() const
{
  return (Address)_offset_low | ((Address)_offset_high << 16)
         | ((Address)_offset_high1 << 32);
}

// interrupt gate
PUBLIC inline
Idt_entry::Idt_entry(Address entry, Unsigned16 selector, Unsigned8 access, Unsigned8 ist_entry = 0)
{
  _offset_low   = entry & 0x000000000000ffffLL;
  _selector     = selector;
  _ist          = ist_entry;
  _access       = access | Access_present;
  _offset_high  = (entry & 0x00000000ffff0000LL) >> 16;
  _offset_high1 = (entry & 0xffffffff00000000LL) >> 32;
  _ignored      = 0;
}


PUBLIC inline
Address
Gdt_entry::base() const
{
  Address b = (Address)base_low | ((Address)base_med  << 16)
              | ((Address)base_high << 24);
  if (access & 0x10)
    return b;

  return b | ((Unsigned64 const *)this)[1] << 32;
}

// task gate
PUBLIC
Idt_entry::Idt_entry(Unsigned16, Unsigned8)
{ panic("AMD64 does not support task gates"); }


IMPLEMENTATION[debug]:

#include <cstdio>


PUBLIC
const char*
X86desc::type_str() const
{
  static char const * const desc_type[32] =
    {
      "reserved",          "16-bit tss (avail)",
      "ldt",               "16-bit tss (busy)",
      "16-bit call gate",  "task gate",
      "16-bit int gate",   "16-bit trap gate",
      "reserved",          "32-bit tss (avail)",
      "reserved",          "32-bit tss (busy)",
      "32-bit call gate",  "reserved",
      "32-bit int gate",   "32-bit trap gate",
      "data r/o",          "data r/o acc",
      "data r/w",          "data r/w acc",
      "data r/o exp-dn",   "data r/o exp-dn",
      "data r/w exp-dn",   "data r/w exp-dn acc",
      "code x/o",          "code x/o acc", 
      "code x/r",          "code x/r acc",
      "code x/r conf",     "code x/o conf acc",
      "code x/r conf",     "code x/r conf acc"
    };

  Unsigned8 const *t = (Unsigned8 const *)this;

  return desc_type[t[5] & 0x1f];
}

PUBLIC
void
Gdt_entry::show() const
{
  static char const modes[] = { 16, 64, 32, -1 };
  // segment descriptor
  Address b = base();
  printf("%016lx-%016lx dpl=%d %dbit %s %02X (\033[33;1m%s\033[m)\n",
         b, b + size(), (access & 0x60) >> 5,
         modes[mode()],
         access & 0x10 ? "code/data" : "system   ",
         (unsigned)access & 0x1f, type_str());
}

PUBLIC inline
unsigned
Idt_entry::selector() const
{ return _selector; }

PUBLIC
void
Idt_entry::show() const
{
  if (type() == 0x5)
    {
      // Task gate

      printf("--------  sel=%04x dpl=%d %02X (\033[33;1m%s\033[m)\n",
             selector(), dpl(), (unsigned)type(), type_str());
    }
  else
    {
      Address o = offset();

      printf("%016lx  sel=%04x dpl=%d %02X (\033[33;1m%s\033[m)\n",
             o, selector(), dpl(), (unsigned)type(), type_str());
    }
}


PUBLIC
void
X86desc::show() const
{
  if (present())
    {
      if ((access() & 0x16) == 0x06)
	static_cast<Idt_entry const*>(this)->show();
      else
	static_cast<Gdt_entry const*>(this)->show();
    }
  else
    {
      printf("--------  dpl=%d %02X (\033[33;1m%s\033[m)\n",
	  dpl(), (unsigned)type(), type_str());
    }
}

IMPLEMENTATION:

#include <cstring>

PUBLIC inline
X86desc::X86desc()
{}

PUBLIC inline
unsigned X86desc::access() const
{ return ((Unsigned8 const *)this)[5]; }

PUBLIC inline NEEDS[X86desc::access]
int
X86desc::present() const
{
  return (access() & 0x80) >> 7;
}

PUBLIC inline NEEDS[X86desc::access]
Unsigned8
X86desc::type() const
{
  return access() & 0x1f;
}

PUBLIC inline NEEDS[X86desc::access]
Unsigned8
X86desc::dpl() const
{
  return (access() & 0x60) >> 5;
}

PUBLIC inline NEEDS[X86desc::present, X86desc::dpl]
bool
X86desc::unsafe() const
{ return present() && (dpl() != 3); }

PUBLIC inline
Pseudo_descriptor::Pseudo_descriptor()
{}

PUBLIC inline
Pseudo_descriptor::Pseudo_descriptor(Address base, Unsigned16 limit)
  : _limit(limit), _base(base)
{}

PUBLIC inline
Address
Pseudo_descriptor::base() const
{
  return _base;
}

PUBLIC inline
unsigned
Idt_entry::ist() const
{ return _ist; }

PUBLIC inline
Unsigned16
Pseudo_descriptor::limit() const
{ return _limit; }

PUBLIC inline NEEDS [<cstring>]
void
Idt_entry::clear()
{ memset(this, 0, sizeof(*this)); }

PUBLIC inline
Gdt_entry::Gdt_entry(Address base, Unsigned32 limit,
		     Unsigned8 _access, Unsigned8 szbits)
{
  limit_low  =  limit & 0x0000ffff;
  base_low   =  base  & 0x0000ffff;
  base_med   = (base  & 0x00ff0000) >> 16;
  access     =  _access | Access_present;
  limit_high = ((limit & 0x000f0000) >> 16) |
			(((Unsigned16)szbits) << 4);
  base_high  = (base  & 0xff000000) >> 24;
}

PUBLIC inline
Gdt_entry::Gdt_entry()
{}


PUBLIC inline
Mword
Gdt_entry::limit() const
{ return (Mword)limit_low | (((Mword)limit_high & 0x0f) << 16); }

PUBLIC inline NEEDS[Gdt_entry::granularity, Gdt_entry::limit]
Mword
Gdt_entry::size() const
{
  Mword l = limit();
  return granularity() ? ((l+1) << 12)-1 : l;
}

PUBLIC inline
bool
Gdt_entry::avl() const
{
  return (limit_high & 0x10);
}

PUBLIC inline
bool
Gdt_entry::seg64() const
{
  return (limit_high & 0x20);
}

PUBLIC inline
bool
Gdt_entry::seg32() const
{
  return (limit_high & 0x40);
}

PUBLIC inline
unsigned
Gdt_entry::mode() const
{ return (limit_high >> 5) & 3; }

PUBLIC inline
bool
Gdt_entry::granularity() const
{
  return (limit_high & 0x80);
}

PUBLIC inline
bool
Gdt_entry::writable() const
{
  return (type() & 0x02);
}

PUBLIC inline
int
Gdt_entry::contents() const
{
  return (type() & 0x0c) >> 2;
}

PUBLIC inline
void
Gdt_entry::clear()
{
  *(Unsigned64*)this = 0;
}

