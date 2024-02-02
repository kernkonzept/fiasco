IMPLEMENTATION [mips]:

IMPLEMENT_OVERRIDE template< typename T >
void *
Jdb_ptab_pdir<T>::entry_virt(T_pte_ptr const &entry) const
{
  return reinterpret_cast<void *>(entry.next_level());
}

IMPLEMENTATION [cpu_mips32]:

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry(T_pte_ptr const &entry) const
{
  if (_dump_raw)
    printf("%08lx", *entry.e);
  else
    {
      if (!entry.is_valid())
        {
          putstr("    -   ");
          return;
        }

      if (!entry.is_leaf())
        {
          printf("%8lx", *entry.e);
          return;
        }

      Address phys = entry.page_addr();

      bool cached = ((*entry.e >> T::PWField_ptei) & Tlb_entry::Cache_mask)
                    == Tlb_entry::cached;

      printf(" %05lx%s%s%c" JDB_ANSI_END,
             phys >> Config::PAGE_SHIFT,
             cached ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
             !(*entry.e & T::XI) ? "" : JDB_ANSI_COLOR(red),
             (*entry.e & T::Write) ? 'w' : 'r');
    }
}


IMPLEMENTATION [cpu_mips64]:

PRIVATE template< typename T >
void
Jdb_ptab_pdir<T>::print_entry(T_pte_ptr const &entry) const
{
  if (_dump_raw)
    printf("%016lx", *entry.e);
  else
    {
      if (!entry.is_valid())
        {
          putstr("        -       ");
          return;
        }

      if (!entry.is_leaf())
        {
          printf("%16lx", *entry.e);
          return;
        }

      Address phys = entry.page_addr();

      bool cached = ((*entry.e >> T::PWField_ptei) & Tlb_entry::Cache_mask)
                    == Tlb_entry::cached;

      printf("%14lx%s%s%c" JDB_ANSI_END,
             phys >> Config::PAGE_SHIFT,
             cached ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
             !(*entry.e & T::XI) ? "" : JDB_ANSI_COLOR(red),
             (*entry.e & T::Write) ? 'w' : 'r');
    }
}
