IMPLEMENTATION:

#include "msrdefs.h"
#include "paging_bits.h"
#include "static_init.h"

class Jdb_kern_info_mtrr : public Jdb_kern_info_module
{
  Address size_or_mask;
};

static Jdb_kern_info_mtrr k_M INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_mtrr::Jdb_kern_info_mtrr()
    : Jdb_kern_info_module('M', "Memory type range registers (MTRRs)")
{
  if (!(Cpu::boot_cpu()->features() & FEAT_MTRR))
    return;

  size_or_mask = ~((1 << (Cpu::boot_cpu()->phys_bits() - Config::PAGE_SHIFT)) - 1);
  Jdb_kern_info::register_subcmd(this);
}

PRIVATE
void
Jdb_kern_info_mtrr::get_var_mtrr(int reg, Address *ret_base,
                                 Address *ret_size, int *ret_type)
{
  Unsigned64 mask, base;
  if (   Jdb::rdmsr(Msr::Ia32_mtrr_phybase1, &mask, 2*reg) && (mask & 0x800)
      && Jdb::rdmsr(Msr::Ia32_mtrr_phybase0, &base, 2*reg))
    {
      *ret_size = (-(size_or_mask | mask >> Config::PAGE_SHIFT))
                    << Config::PAGE_SHIFT;
      *ret_base = Pg::trunc(base);
      *ret_type = base & 0x0f;
    }
  else
    *ret_size = 0;
}

PUBLIC
void
Jdb_kern_info_mtrr::show() override
{
  static const char * const typestr[] =
  {
    "uncacheable (UC)", "write combining (WC)", "??", "??",
    "write-through (WT)", "write-protected (WP)", "write back (WB)", "??"
  };
  Unsigned64 num_mtrr;
  if (Jdb::rdmsr(Msr::Ia32_mtrrcap, &num_mtrr))
    num_mtrr &= 0xff;
  else
    num_mtrr = 8;
  for (unsigned i = 0; i < num_mtrr; ++i)
    {
      Address base, size;
      int type = 0;
      get_var_mtrr(i, &base, &size, &type);
      if (size)
        printf(" %2d: " L4_PTR_FMT "-" L4_PTR_FMT " (%lu%cB) %s\n",
               i, base, base+size,
               size >= 8 << 20 ? (size+(1<<19)-1) >> 20 : (size+(1<<9)-1) >> 10,
               size >= 8 << 20 ? 'M' : 'K',
               typestr[type]);
    }
}
