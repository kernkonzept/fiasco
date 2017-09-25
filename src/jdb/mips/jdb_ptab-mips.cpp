IMPLEMENTATION [mips]:

IMPLEMENT_OVERRIDE
void *
Jdb_ptab::entry_virt(Pdir::Pte_ptr const &entry)
{
  return (void *)entry.next_level();
}

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

      char const pss[] = "CS";
      char ps = pss[entry.level];

      printf(" %05lx%s%c", phys >> Config::PAGE_SHIFT,
                           ((*entry.e >> Pdir::PWField_ptei) & Tlb_entry::Cache_mask) == Tlb_entry::Cached
                            ? "-" : JDB_ANSI_COLOR(lightblue) "n" JDB_ANSI_END,
                           ps);
      printf("%s%c" JDB_ANSI_END,
             !(*entry.e & Pdir::XI) ? "" : JDB_ANSI_COLOR(red),
             (*entry.e & Pdir::Write) ? 'w' : 'r');
    }
}


