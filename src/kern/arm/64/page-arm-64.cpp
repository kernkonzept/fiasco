//---------------------------------------------------------------------------
INTERFACE [arm && mmu && cpu_virt && !arm_pt48]:

EXTENSION class Page
{
public:
  enum
  {
    Min_pa_range = 0, // Can be used for everything smaller than 40 bits.
    Max_pa_range = 2, // 40 bits PA/IPA size (encoded as VTCR_EL2.PS)
    Vtcr_sl0 = 1,     // 3 level page table
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && cpu_virt && arm_pt48]:

EXTENSION class Page
{
public:
  enum
  {
    Min_pa_range = 4, // 4-level stage2 PTs need at least 44 (I)PA bits!
    Max_pa_range = 5, // 48 bits PA/IPA size (encoded as VTCR_EL2.PS)
    Vtcr_sl0 = 2,     // 4 level page table
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && cpu_virt]:

#include "cpubits.h"
#include "mem_unit.h"

EXTENSION class Page
{
public:
  enum
  {
    Ttbcr_bits =   (1UL << 31) | (1UL << 23) // RES1
                 | (Tcr_attribs <<  8) // (IRGN0)
                 | (16UL <<  0) // (T0SZ) Address space size 48 bits (64 - 16)
                 | (0UL  << 14) // (TG0)  Page granularity 4 KiB
                 | (5UL  << 16) // (PS)   Physical address size 48 bits
  };

  static unsigned inline ipa_bits(unsigned pa_range)
  {
    if (pa_range > Max_pa_range)
      pa_range = Max_pa_range;

    return Cpubits::pa_range_bits[pa_range];
  }

  static unsigned inline vtcr_bits(unsigned pa_range)
  {
    if (pa_range > Max_pa_range)
      pa_range = Max_pa_range;

    unsigned pa_bits = ipa_bits(pa_range);

    return (Vtcr_sl0            <<  6)  // SL0
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
