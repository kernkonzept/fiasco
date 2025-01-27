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

PRIVATE static
void
Jdb_kern_info_mtrr::show_fixed_mtrr(Msr msr, unsigned size_kb, unsigned addr)
{
  static constexpr const char *const typestr[] =
  { "UC", "WC", "??", "??", "WT", "WP", "WB", "??" };

  Unsigned64 value;
  if (Jdb::rdmsr(msr, &value))
    printf("  %05x (%2ukB): %s %s %s %s %s %s %s %s\n",
           addr, size_kb,
           typestr[(value >> 56) & 7],
           typestr[(value >> 48) & 7],
           typestr[(value >> 40) & 7],
           typestr[(value >> 32) & 7],
           typestr[(value >> 24) & 7],
           typestr[(value >> 16) & 7],
           typestr[(value >>  8) & 7],
           typestr[(value      ) & 7]);
}

PUBLIC
void
Jdb_kern_info_mtrr::show() override
{
  static constexpr const char *const typestr[] =
  {
    "uncacheable (UC)", "write combining (WC)", "??", "??",
    "write-through (WT)", "write-protected (WP)", "write back (WB)", "??"
  };
  Unsigned64 mtrr_cap;
  unsigned num_mtrr;
  if (Jdb::rdmsr(Msr::Ia32_mtrrcap, &mtrr_cap))
    num_mtrr = mtrr_cap & 0xff;
  else
    num_mtrr = 8;
  Unsigned64 mtrr_def_type = 0;
  Jdb::rdmsr(Msr::Ia32_mtrr_def_type, &mtrr_def_type);
  if (mtrr_def_type & 0x800)
    {
      printf("MTRR default type: %s\n", typestr[mtrr_def_type & 7]);
      printf("Fixed-range MTRRs:\n");
      if ((mtrr_cap & 0x100) && (mtrr_def_type & 0x400))
        {
          // IA32_MTRR_FIX64K_...
          show_fixed_mtrr(Msr{0x250}, 64, 0x00000);
          // IA32_MTRR_FIX16K_...
          show_fixed_mtrr(Msr{0x258}, 16, 0x80000);
          show_fixed_mtrr(Msr{0x259}, 16, 0xa0000);
          // IA32_MTRR_FIX4K_...
          show_fixed_mtrr(Msr{0x268},  4, 0xc0000);
          show_fixed_mtrr(Msr{0x269},  4, 0xc8000);
          show_fixed_mtrr(Msr{0x26a},  4, 0xd0000);
          show_fixed_mtrr(Msr{0x26b},  4, 0xd8000);
          show_fixed_mtrr(Msr{0x26c},  4, 0xe0000);
          show_fixed_mtrr(Msr{0x26d},  4, 0xe8000);
          show_fixed_mtrr(Msr{0x26e},  4, 0xf0000);
          show_fixed_mtrr(Msr{0x26f},  4, 0xf8000);
        }
      else
        printf("  disabled!\n");
      printf("Variable-range MTRRs:\n");
      for (unsigned i = 0; i < num_mtrr; ++i)
        {
          Address base, size;
          int type = 0;
          get_var_mtrr(i, &base, &size, &type);
          if (size)
            printf(" %2d: " L4_PTR_FMT "-" L4_PTR_FMT " (%lu%cB) %s\n",
                   i, base, base + size,
                   size >= 8 << 20 ? (size + (1<<19) - 1) >> 20
                                   : (size +  (1<<9) - 1) >> 10,
                   size >= 8 << 20 ? 'M' : 'K',
                   typestr[type]);
        }
    }
  else
    printf("MTRRs disabled in Msr::Ia32_mtrr_def_type\n");
}
