INTERFACE:

#include "config_gdt.h"
#include "l4_types.h"
#include "x86desc.h"

class Gdt
{
public:
   /** Segment numbers. */
  static constexpr Unsigned16 gdt_tss         = GDT_TSS;
  static constexpr Unsigned16 gdt_code_kernel = GDT_CODE_KERNEL;
  static constexpr Unsigned16 gdt_data_kernel = GDT_DATA_KERNEL;
  static constexpr Unsigned16 gdt_code_user   = GDT_CODE_USER;
  static constexpr Unsigned16 gdt_data_user   = GDT_DATA_USER;
  static constexpr Unsigned16 gdt_tss_dbf     = GDT_TSS_DBF;
  static constexpr Unsigned16 gdt_utcb        = GDT_UTCB;
  static constexpr Unsigned16 gdt_ldt         = GDT_LDT;
  static constexpr Unsigned16 gdt_user_entry1 = GDT_USER_ENTRY1;
  static constexpr Unsigned16 gdt_user_entry2 = GDT_USER_ENTRY2;
  static constexpr Unsigned16 gdt_user_entry3 = GDT_USER_ENTRY3;
  static constexpr Unsigned16 gdt_user_entry4 = GDT_USER_ENTRY4;
  static constexpr Unsigned16 gdt_code_user32 = GDT_CODE_USER32;
  static constexpr Unsigned16 gdt_max         = GDT_MAX;

  enum
  {
    Selector_user       = 0x03,
    Selector_kernel     = 0x00,
  };

private:
  Gdt_entry _entries[0];
};

//------------------------------------------------------------------
IMPLEMENTATION [ia32]:

/**
 * Setup a TSS entry.
 *
 * The entry is set up as TSS Available and usable by the kernel in the GDT.
 *
 * \param nr     Selector number.
 * \param base   Address of the TSS.
 * \param limit  Limit of the TSS (in bytes).
 */
PUBLIC inline
void
Gdt::set_entry_tss(unsigned nr, Address base, Unsigned32 limit)
{
  _entries[nr] = Gdt_entry(base, limit, Gdt_entry::Tss_available,
                           Gdt_entry::Kernel, Gdt_entry::Granularity_bytes);
}

//------------------------------------------------------------------
IMPLEMENTATION [amd64]:

/**
 * Setup a TSS entry.
 *
 * The entry is set up as TSS Available and usable by the kernel in the GDT.
 *
 * \note The TSS actually occupies two consecutive GDT entries.
 *
 * \param nr     Selector number.
 * \param base   Address of the TSS.
 * \param limit  Limit of the TSS (in bytes).
 */
PUBLIC inline
void
Gdt::set_entry_tss(unsigned nr, Address base, Unsigned32 limit)
{
  _entries[nr] = Gdt_entry(base, limit, Gdt_entry::Tss_available,
                           Gdt_entry::Kernel, Gdt_entry::Granularity_bytes);
  _entries[nr + 1] = Gdt_entry(base);
}

//------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline explicit
Gdt::Gdt(unsigned nr_entries = Gdt::gdt_max / sizeof(Gdt_entry))
{
  for (unsigned i = 0; i < nr_entries; ++i)
    _entries[i] = Gdt_entry();
}

PUBLIC inline
void
Gdt::set_entry_4k(unsigned nr, Address base, Unsigned32 limit,
                  Gdt_entry::Access access, Gdt_entry::Type type,
                  Gdt_entry::Dpl dpl, Gdt_entry::Code code,
                  Gdt_entry::Default_size default_size)
{
  _entries[nr] = Gdt_entry(base, limit >> 12, access, type, dpl, code,
                           default_size, Gdt_entry::Granularity_4k);
}

PUBLIC inline
void
Gdt::set_entry_ldt(unsigned nr, Address base, Unsigned32 limit)
{
  _entries[nr] = Gdt_entry(base, limit, Gdt_entry::Ldt, Gdt_entry::Kernel,
                           Gdt_entry::Granularity_bytes);
}

PUBLIC inline
void
Gdt::clear_entry(unsigned nr)
{
  _entries[nr] = Gdt_entry();
}

PUBLIC inline
Gdt_entry *
Gdt::entries()
{
  return _entries;
}

PUBLIC inline
Gdt_entry &
Gdt::operator [](unsigned idx)
{
  return _entries[idx];
}

PUBLIC inline
Gdt_entry const &
Gdt::operator [](unsigned idx) const
{
  return _entries[idx];
}

IMPLEMENTATION[ia32 || amd64]:

PUBLIC static inline
void
Gdt::set(Pseudo_descriptor *desc)
{
  asm volatile ("lgdt %0" : : "m" (*desc));
}

PUBLIC static inline
void
Gdt::get(Pseudo_descriptor *desc)
{
  asm volatile ("sgdt %0" : "=m" (*desc) : : "memory");
}

PUBLIC static inline
int
Gdt::data_segment()
{
  return gdt_data_user | Selector_user;
}
