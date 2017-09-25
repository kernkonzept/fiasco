IMPLEMENTATION [ia32,ux,arm]:

#include "paging.h"

IMPLEMENTATION [ia32,ux]:

IMPLEMENT
void
Jdb_ptab::print_entry(Pdir::Pte_ptr const &entry)
{
  if (dump_raw)
    {
      printf(L4_PTR_FMT, *entry.pte);
      return;
    }

  if (!entry.is_valid())
    {
      putstr("    -   ");
      return;
    }

  Address phys = entry_phys(entry);

  if (entry.level != Pdir::Depth && entry.is_leaf())
    printf((phys >> 20) > 0xFF
	    ? "%03lX/4" : " %02lX/4", phys >> 20);
  else
    printf((phys >> Config::PAGE_SHIFT) > 0xFFFF
	    ? "%05lx" : " %04lx", phys >> Config::PAGE_SHIFT);

  putchar(((cur_pt_level >= Pdir::Depth || entry.is_leaf()) &&
	  (*entry.pte & Pt_entry::Cpu_global)) ? '+' : '-');
  printf("%s%c%s", *entry.pte & Pt_entry::Noncacheable ? JDB_ANSI_COLOR(lightblue) : "",
                   *entry.pte & Pt_entry::Noncacheable
	            ? 'n' : (*entry.pte & Pt_entry::Write_through) ? 't' : '-',
                   *entry.pte & Pt_entry::Noncacheable ? JDB_ANSI_END : "");
  putchar(*entry.pte & Pt_entry::User
	    ? (*entry.pte & Pt_entry::Writable) ? 'w' : 'r'
	    : (*entry.pte & Pt_entry::Writable) ? 'W' : 'R');
}

