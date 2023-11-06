IMPLEMENTATION [riscv]:

#include "paging.h"

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry(T_pte_ptr const &entry) const
{
  if (_dump_raw)
    {
      printf(L4_PTR_FMT, *entry.pte);
      return;
    }

  if (!entry.is_valid())
  {
    printf("%*s-%*s", static_cast<int>(sizeof(Address)), "",
           static_cast<int>(sizeof(Address) - 1), "");
    return;
  }

  print_entry_addr(entry);

  auto attr = entry.attribs();
  putchar(entry.is_leaf() && (attr.kern & Page::Kern::Global()) ? '+' : '-');

  auto user = attr.rights & Page::Rights::U();
  auto r = attr.rights & Page::Rights::R();
  auto w = r && (attr.rights & Page::Rights::W());
  auto x = attr.rights & Page::Rights::X();

  putchar( w ? (user ? 'w' : 'W')
             : r ? (user ? 'r' : 'R')
                 : '-' );

  putchar( x ? (user ? 'x' : 'X') : '-');
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && 32bit]:

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry_addr(T_pte_ptr const &entry) const
{
  Address phys = entry_phys(entry);
  if (entry.level != T::Depth && entry.is_leaf())
    // 4 MB page entry
    printf("%03lx/4", phys >> entry.page_order());
  else
    // point to 4K page (leaf in lowest level or point to next level)
    printf("%05lx", phys >> Config::PAGE_SHIFT);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && 64bit]:

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry_addr(T_pte_ptr const &entry) const
{
  Address phys = entry_phys(entry);
  auto level = T::Depth - entry.level;
  if (!entry.is_leaf())
    {
      printf("%13lx", phys >> Config::PAGE_SHIFT);
    }
  else if (level == 0)
    {
      printf("%11lx/4", phys >> entry.page_order());
    }
  else if (level == 1)
    {
      printf(" %9lx/2M", phys >> entry.page_order());
    }
  else if (level == 2)
    {
      printf("   %7lx/1G", phys >> entry.page_order());
    }
  else if (level == 3)
    {
      printf("   %5lx/512G", phys >> entry.page_order());
    }
}
