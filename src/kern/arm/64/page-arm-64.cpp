//---------------------------------------------------------------------------
INTERFACE [arm && mmu && cpu_virt]:

#include "cpubits.h"
#include "mem_unit.h"
#include "minmax.h"

EXTENSION class Page
{
public:
  enum
  {
    Max_pa_range_3lvl = 2, // 40 bits PA/IPA size (encoded as VTCR_EL2.PS)
    Vtcr_sl0_3lvl = 1,     // 3 level page table

    Max_pa_range_4lvl = 5, // 48 bits PA/IPA size (encoded as VTCR_EL2.PS)
    Vtcr_sl0_4lvl = 2,     // 4 level page table

    Ttbcr_bits =   (1UL << 31) | (1UL << 23) // RES1
                 | (Tcr_attribs <<  8) // (IRGN0)
                 | (16UL <<  0) // (T0SZ) Address space size 48 bits (64 - 16)
                 | (0UL  << 14) // (TG0)  Page granularity 4 KiB
                 | (5UL  << 16) // (PS)   Physical address size 48 bits
  };

  static unsigned check_pa_range(unsigned pa_range)
  {
    if (Cpubits::Pt4())
      return min<unsigned>(pa_range, Max_pa_range_4lvl);
    else
      return min<unsigned>(pa_range, Max_pa_range_3lvl);
  }

  static unsigned inline ipa_bits(unsigned pa_range)
  {
    pa_range = check_pa_range(pa_range);
    return Cpubits::pa_range_bits[pa_range];
  }

  static unsigned vtcr_sl0()
  {
    return Cpubits::Pt4() ? Vtcr_sl0_4lvl : Vtcr_sl0_3lvl;
  }

  static unsigned inline vtcr_bits(unsigned pa_range)
  {
    pa_range = check_pa_range(pa_range);
    unsigned pa_bits = ipa_bits(pa_range);

    return (vtcr_sl0()          <<  6)  // SL0
            | (pa_range         << 16)  // PS
            | ((64U - pa_bits)  <<  0)  // T0SZ
            | ((Mem_unit::Asid_bits == 16) << 19); // VS
  }
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !cpu_virt]:

#include "mem_unit.h"

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (Tcr_attribs <<  8) // (IRGN0)
                 | (Tcr_attribs << 24) // (IRGN1)
                 | (16UL <<  0) // (T0SZ) Address space size 48 bits (64 - 16)
                 | (16UL << 16) // (T1SZ) Address space size 48 bits (64 - 16)
                 | (0UL  << 14) // (TG0)  Page granularity 4 KiB
                 | (2UL  << 30) // (TG1)  Page granularity 4 KiB
                 | (5UL  << 32) // (IPS)  Physical address size 48 bits
                                // (AS)   ASID Size
                 | ((Mem_unit::Asid_bits == 16 ? 1UL : 0UL) << 36)
  };
};
