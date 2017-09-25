IMPLEMENTATION:

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
  Unsigned64 mask = Cpu::rdmsr(0x201 + 2*reg);
  Unsigned64 base;

  if ((mask & 0x800) == 0)
    {
      /* MTRR not active */
      *ret_size = 0;
      return;
    }

  Jdb::msr_fail = 0;
  Jdb::msr_test = Jdb::Msr_test_fail_ignore;
  base = Cpu::rdmsr(0x200 + 2*reg);
  Jdb::msr_test = Jdb::Msr_test_default;
  if (Jdb::msr_fail)
    {
      /* invalid MSR */
      *ret_size = 0;
      return;
    }

  *ret_size = (-(size_or_mask | mask >> Config::PAGE_SHIFT))
               << Config::PAGE_SHIFT;
  *ret_base = base & Config::PAGE_MASK;
  *ret_type = base & 0x0f;
}

PUBLIC
void
Jdb_kern_info_mtrr::show()
{
  int num_mtrr;
  static const char * const typestr[] = 
    {
      "uncacheable (UC)", "write combining (WC)", "??", "??",
      "write-through (WT)", "write-protected (WP)", "write back (WB)", "??"
    };
  Jdb::msr_fail = 0;
  Jdb::msr_test = Jdb::Msr_test_fail_ignore;
  num_mtrr = Cpu::rdmsr(0xfe) & 0xff;
  Jdb::msr_test = Jdb::Msr_test_default;
  if (Jdb::msr_fail)
    num_mtrr = 8;
  for (int i=0; i<num_mtrr; i++)
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
