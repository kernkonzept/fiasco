IMPLEMENTATION [mips]:

IMPLEMENT_OVERRIDE
void *
Jdb_ptab::entry_virt(Pdir::Pte_ptr const &entry)
{
  return (void *)entry.next_level();
}

IMPLEMENTATION [cpu_mips32]:

IMPLEMENT
void
Jdb_ptab::print_entry(Pdir::Pte_ptr const &entry)
{
  if (dump_raw)
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

      bool cached = ((*entry.e >> Pdir::PWField_ptei) & Tlb_entry::Cache_mask)
                    == Tlb_entry::cached;

      printf(" %05lx%s%s%c" JDB_ANSI_END,
             phys >> Config::PAGE_SHIFT,
             cached ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
             !(*entry.e & Pdir::XI) ? "" : JDB_ANSI_COLOR(red),
             (*entry.e & Pdir::Write) ? 'w' : 'r');
    }
}


IMPLEMENTATION [cpu_mips64]:

IMPLEMENT
void
Jdb_ptab::print_entry(Pdir::Pte_ptr const &entry)
{
  if (dump_raw)
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

      bool cached = ((*entry.e >> Pdir::PWField_ptei) & Tlb_entry::Cache_mask)
                    == Tlb_entry::cached;

      printf("%14lx%s%s%c" JDB_ANSI_END,
             phys >> Config::PAGE_SHIFT,
             cached ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
             !(*entry.e & Pdir::XI) ? "" : JDB_ANSI_COLOR(red),
             (*entry.e & Pdir::Write) ? 'w' : 'r');
    }
}
