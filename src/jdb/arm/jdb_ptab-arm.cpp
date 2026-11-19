IMPLEMENTATION [arm]:

#include "paging.h"
#include "simpleio.h"

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

PROTECTED inline template< typename T >
bool
Jdb_ptab_pdir<T>::is_cached(Pte_ptr const &entry) const
{
  if (entry.level == 0 && (*entry.pte & 3) != 2)
    return true; /* No caching options on PDEs */
  return (*entry.pte & Page::Cache_mask) == Page::CACHEABLE;
}

PROTECTED inline template< typename T >
bool
Jdb_ptab_pdir<T>::is_executable(Pte_ptr const &) const
{
  return true;
}

PROTECTED inline template< typename T >
char
Jdb_ptab_pdir<T>::ap_char(unsigned ap) const
{
  return ap & 0x2 ? (ap & 0x1) ? 'w' : 'r'
                  : (ap & 0x1) ? 'W' : 'R';
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus && !arm_lpae]:

PROTECTED inline template< typename T >
bool
Jdb_ptab_pdir<T>::is_cached(Pte_ptr const &entry) const
{
  if (entry.level == 0)
    {
      if ((*entry.pte & 3) == 2)
        return (*entry.pte & Page::Section_cache_mask) == Page::Section_cachable_bits;
      return true;
    }

  return (*entry.pte & Page::Cache_mask) == Page::CACHEABLE;
}

PROTECTED inline template< typename T>
bool
Jdb_ptab_pdir<T>::is_executable(Pte_ptr const &entry) const
{
  // section: XN is bit 4; large page: XN is bit 15; small page: XN is bit 0
  if (entry.level == 0)
    return !(*entry.pte & 0x10);
  if ((*entry.pte & 3) == 1)
    return !(*entry.pte & 0x8000);
  return !(*entry.pte & 1);
}

PROTECTED inline template< typename T >
char
Jdb_ptab_pdir<T>::ap_char(unsigned ap) const
{
  switch (ap & 0x23)
  {
    case    0: return '-';
    case    1: return 'W';
    case    2: return 'r';
    case    3: return 'w';
    case 0x21: return 'R';
    case 0x22: case 0x23: return 'r';
    default: return '?';
  };
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry(T_pte_ptr const &entry) const
{
  if (_dump_raw)
    printf("%08x", *entry.pte);
  else
    {
      if (!entry.is_valid())
        {
          putstr("    -   ");
          return;
        }
      Address phys = entry_phys(entry);

      unsigned t = *entry.pte & 0x03;
      unsigned ap = *entry.pte >> 4;
      char ps;
      if (entry.level == 0)
        switch (t)
          {
          case 1: ps = 'C'; break;
          case 2: ps = 'S'; ap = *entry.pte >> 10; break;
          case 3: ps = 'F'; break;
          default: ps = 'U'; break;
          }
      else
        switch (t)
          {
          case 1: ps = 'l'; break;
          case 2: ps = 's'; break;
          case 3: ps = 't'; break;
          default: ps = 'u'; break;
          }

      printf("%05lx%s%c", phys >> Config::PAGE_SHIFT,
                          is_cached(entry)
                           ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
                          ps);
      if (entry.level == 0 && t != 2)
        putchar('-');
      else
        printf("%s%c" JDB_ANSI_END,
               is_executable(entry) ? "" : JDB_ANSI_COLOR(red),
               ap_char(ap));
    }
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && !cpu_virt]:

PROTECTED inline template< typename T >
char
Jdb_ptab_pdir<T>::ap_char(T_pte_ptr const &entry) const
{
  static char const ch[] = { 'W', 'w', 'R', 'r' };
  return ch[(*entry.pte >> 6) & 3];
}

/**
 * Whether the entry is executable at the privilege level it is meant for.
 *
 * Kernel mappings always have UXN set and user mappings always have PXN set,
 * so only the execute-never bit of the level the mapping is intended for is
 * meaningful (see Pte_long_attribs::_attribs()). AP[1] selects which one that
 * is.
 */
PROTECTED inline template< typename T >
bool
Jdb_ptab_pdir<T>::is_executable(T_pte_ptr const &entry) const
{
  Unsigned64 xn = Page::XN | ((*entry.pte & 0x40) ? Page::UXN : Page::PXN);
  return !(*entry.pte & xn);
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && cpu_virt]:

PROTECTED inline template< typename T >
char
Jdb_ptab_pdir<T>::ap_char(T_pte_ptr const &entry) const
{
  // Kpdir: AP[2] means read-only; Pdir (stage 2): S2AP[1] means writable
  bool bit7 = (*entry.pte >> 7) & 1;
  if constexpr (cxx::is_same_v<T, Kpdir>)
    return bit7 ? 'r' : 'w';
  else
    return bit7 ? 'w' : 'r';
}

/**
 * Whether the entry is executable.
 *
 * Both the EL2 translation regime used for Kpdir and the stage-2 page table
 * used for Pdir know a single execute-never bit only.
 */
PROTECTED inline template< typename T >
bool
Jdb_ptab_pdir<T>::is_executable(T_pte_ptr const &entry) const
{
  return !(*entry.pte & (1ULL << 54));
}

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PROTECTED inline template< typename T >
bool
Jdb_ptab_pdir<T>::is_cached(T_pte_ptr const &entry) const
{
  enum
  {
    Cache_mask = cxx::is_same_v<T, Kpdir>
                   ? Mword{Kernel_page_attr::Cache_mask}
                   : Mword{Page::Cache_mask},
    CACHEABLE  = cxx::is_same_v<T, Kpdir>
                   ? Mword{Kernel_page_attr::CACHEABLE}
                   : Mword{Page::CACHEABLE}
  };
  if (!entry.is_leaf())
    return true;
  return (*entry.pte & Cache_mask) == CACHEABLE;
}

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry(T_pte_ptr const &entry) const
{
  if (_dump_raw)
    printf("%16llx", *entry.pte);
  else
    {
      if (!entry.is_valid())
        {
          putstr("        -       ");
          return;
        }

      Address phys = entry_phys(entry);

      char ps;
      if (!entry.is_leaf())
        ps = 'P';
      else
        switch (entry.page_order())
          {
          case 12: ps = 'p'; break;
          case 21: ps = 'M'; break;
          case 30: ps = 'G'; break;
          default: ps = '?'; break;
          }

      printf("%13lx%s%c", phys >> Config::PAGE_SHIFT,
                          is_cached(entry)
                           ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
                          ps);
      if (!entry.is_leaf())
        putchar('-');
      else
        printf("%s%c" JDB_ANSI_END,
               is_executable(entry) ? "" : JDB_ANSI_COLOR(red),
               ap_char(entry));
    }
}

IMPLEMENT_OVERRIDE
void
Jdb_ptab_m::show_ptab(void *ptab_base, Space *s, int level = 0)
{
  if (s == Kernel_task::kernel_task())
    {
      Jdb_ptab_pdir<Kpdir> pt_view(ptab_base, s, 0, 0, level);
      pt_view.show(0, 1);
    }
  else
    {
      Jdb_ptab_pdir<Pdir> pt_view(ptab_base, s, 0, 0, level);
      pt_view.show(0, 1);
    }
}
