INTERFACE [arm && 64bit]:

#include "types.h"

class Cpubits
{
public:
  static char constexpr pa_range_bits[16] = { 32, 36, 40, 42, 44, 48, 52, 56 };
};

// ------------------------------------------------------------------------
INTERFACE [arm && !arm_single_pt]:

#include "alternatives.h"

EXTENSION class Cpubits
{
public:
  // Whether we are using 3-level or 4-level page-tables
  struct Pt4 : public Alternative_static_functor<Pt4>
  {
    static bool probe()
    {
      Mword id_aa64mmfr0_el1;
      asm("mrs %0, S3_0_C0_C7_0" : "=r"(id_aa64mmfr0_el1));
      return (id_aa64mmfr0_el1 & 0xfU) >= 4;
    }
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_single_pt]:

EXTENSION class Cpubits
{
public:
  struct Pt4
  {
    constexpr operator bool() { return TAG_ENABLED(arm_pt48_only); }
  };
};
