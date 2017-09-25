
INTERFACE:

#include "config_gdt.h"
#include "l4_types.h"
#include "x86desc.h"

class Gdt
{
public:
   /** Segment numbers. */
  enum 
  {
    gdt_tss             = GDT_TSS,
    gdt_code_kernel     = GDT_CODE_KERNEL,
    gdt_data_kernel     = GDT_DATA_KERNEL,
    gdt_code_user       = GDT_CODE_USER,
    gdt_data_user       = GDT_DATA_USER,
    gdt_tss_dbf         = GDT_TSS_DBF,
    gdt_utcb            = GDT_UTCB,
    gdt_ldt             = GDT_LDT,
    gdt_user_entry1     = GDT_USER_ENTRY1,
    gdt_user_entry2     = GDT_USER_ENTRY2,
    gdt_user_entry3     = GDT_USER_ENTRY3,
    gdt_user_entry4     = GDT_USER_ENTRY4,
    gdt_code_user32     = GDT_CODE_USER32,
    gdt_max             = GDT_MAX,
  };

  enum
  {
    Selector_user       = 0x03,
    Selector_kernel     = 0x00,
  };

private:
  Gdt_entry _entries[0];
};

//------------------------------------------------------------------
IMPLEMENTATION [amd64]:

PUBLIC inline
void
Gdt::set_entry_tss(int nr, Address base, Unsigned32 limit,
		   Unsigned8 access, Unsigned8 szbits)
{
  // system-segment descriptor is 16byte
  _entries[nr] = Gdt_entry(base, limit >> 12, access, szbits | 0x08);
  _entries[nr + 1].raw = base >> 32;
}

//------------------------------------------------------------------
IMPLEMENTATION:


PUBLIC inline
void
Gdt::set_entry_byte(int nr, Address base, Unsigned32 limit,
		    Unsigned8 access, Unsigned8 szbits)
{
  _entries[nr] = Gdt_entry(base, limit, access, szbits);
}

PUBLIC inline
void
Gdt::set_entry_4k(int nr, Address base, Unsigned32 limit,
		  Unsigned8 access, Unsigned8 szbits)
{
  _entries[nr] = Gdt_entry(base, limit >> 12, access, szbits | 0x08);
}

PUBLIC inline
void
Gdt::clear_entry(int nr)
{
  _entries[nr].clear();
}

PUBLIC inline
Gdt_entry*
Gdt::entries()
{
  return _entries;
}

PUBLIC inline
Gdt_entry &
Gdt::operator [] (unsigned idx)
{ return _entries[idx]; }

PUBLIC inline
Gdt_entry const &
Gdt::operator [] (unsigned idx) const
{ return _entries[idx]; }


IMPLEMENTATION[ia32 | amd64]:

PUBLIC static inline
void
Gdt::set (Pseudo_descriptor *desc)
{
  asm volatile ("lgdt %0" : : "m" (*desc));
}

PUBLIC static inline
void
Gdt::get (Pseudo_descriptor *desc)
{
  asm volatile ("sgdt %0" : "=m" (*desc) : : "memory");
}

PUBLIC static inline
int
Gdt::data_segment ()
{ return gdt_data_user | Selector_user; }
