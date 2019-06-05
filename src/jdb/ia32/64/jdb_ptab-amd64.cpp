IMPLEMENTATION [amd64]:

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
      putstr("        -       ");
      return;
    }

  Address phys = entry_phys(entry);

  if (entry.level != T::Depth && entry.is_leaf())
    printf((phys >> 20) > 0xFF
	   ? "       %03lx/2" : "        %02lx/2", phys >> 20);
  else
    // truncates the upper 4bit of the physical address,
    // which are attributes anyways
    printf((phys >> Config::PAGE_SHIFT) > 0xFFFF
           ? "%12lx" : "        %04lx", phys >> Config::PAGE_SHIFT);

  putchar(((_pt_level >= T::Depth || entry.is_leaf()) &&
	 (*entry.pte & Pt_entry::Cpu_global)) ? '+' : '-');
  printf("%s%c%s", *entry.pte & Pt_entry::Noncacheable ? JDB_ANSI_COLOR(lightblue) : "",
                   *entry.pte & Pt_entry::Noncacheable
                    ? 'n' : (*entry.pte & Pt_entry::Write_through) ? 't' : '-',
                   *entry.pte & Pt_entry::Noncacheable ? JDB_ANSI_END : "");
  putchar(*entry.pte & Pt_entry::User
	    ? (*entry.pte & Pt_entry::Writable) ? 'w' : 'r'
	    : (*entry.pte & Pt_entry::Writable) ? 'W' : 'R');
  putchar(*entry.pte & Pt_entry::XD ? '-' : 'x');
}



