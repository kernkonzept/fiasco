INTERFACE:

#include "types.h"

/*
 * These definitions are separate from Cpu to avoid a cyclic dependency between
 * Mem_unit and Cpu when Mem_unit depends on CPU-related functions / constants.
 */

class Cpudefs
{
public:
  enum
  {
    Midr_impl_arm = 0x41,
    Midr_arch_new = 0xf,
    Midr_mask = (0xff << 24) | (0xf << 16) | (0xfff << 4)
  };

  enum Midr_part
  {
    Part_foundatation = 0xd00,
    Part_aem_v8 = 0xd0f,

    Part_arm926 = 0x926,
    Part_arm1136j_s = 0xb36,
    Part_arm1176jz_s = 0xb76,

    Part_c1_premium = 0xd90,
    Part_c1_ultra = 0xd8c,

    Part_cortex_a5 = 0xc05,
    Part_cortex_a7 = 0xc07,
    Part_cortex_a8 = 0xc08,
    Part_cortex_a9 = 0xc09,
    Part_cortex_a12 = 0xc0d,
    Part_cortex_a15 = 0xc0f,
    Part_cortex_a17 = 0xc0e,
    Part_cortex_a35 = 0xd04,
    Part_cortex_a53 = 0xd03,
    Part_cortex_a55 = 0xd05,
    Part_cortex_a57 = 0xd07,
    Part_cortex_a72 = 0xd08,
    Part_cortex_a73 = 0xd09,
    Part_cortex_a75 = 0xd0a,
    Part_cortex_a76 = 0xd0b,
    Part_cortex_a76ae = 0xd0e,
    Part_cortex_a77 = 0xd0d,
    Part_cortex_r52 = 0xd13,
    Part_cortex_a78 = 0xd41,
    Part_cortex_a78c = 0xd4b,
    Part_cortex_a78ae = 0xd42,
    Part_cortex_a510 = 0xd46,
    Part_cortex_a520 = 0xd80,
    Part_cortex_a520ae = 0xd88,
    Part_cortex_a710 = 0xd47,
    Part_cortex_a720 = 0xd81,
    Part_cortex_a720ae = 0xd89,
    Part_cortex_a725 = 0xd87,

    Part_cortex_r82 = 0xd15,

    Part_cortex_x1 = 0xd44,
    Part_cortex_x1c = 0xd4c,
    Part_cortex_x2 = 0xd48,
    Part_cortex_x3 = 0xd4e,
    Part_cortex_x4 = 0xd82,
    Part_cortex_x925 = 0xd85,

    Part_neoverse_n1 = 0xd0c,
    Part_neoverse_n2 = 0xd49,
    Part_neoverse_n3 = 0xd8e,
    Part_neoverse_v1 = 0xd40,
    Part_neoverse_v2 = 0xd4f,
    Part_neoverse_v3 = 0xd84,
    Part_neoverse_e1 = 0xd4a,
    Part_neoverse_v3ae = 0xd83,
  };

  static constexpr Unsigned32 Midr(unsigned impl, unsigned partnum)
  {
    return (impl << 24) | (Midr_arch_new << 16) | (partnum << 4);
  }

  static constexpr Unsigned32 Midr_arm(unsigned partnum)
  {
    return Midr(Midr_impl_arm, partnum);
  }
};

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

PUBLIC static inline
Mword
Cpudefs::midr()
{
  Mword midr;
  asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (midr));
  return midr;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

PUBLIC static inline
Mword
Cpudefs::midr()
{
  Mword midr;
  asm volatile ("mrs %0, midr_el1" : "=r" (midr));
  return midr;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

PUBLIC static inline
bool
Cpudefs::needs_workaround_repeat_tlbi()
{
  switch (midr() & Midr_mask)
    {
    case Midr_arm(Part_c1_premium):
    case Midr_arm(Part_c1_ultra):
    case Midr_arm(Part_cortex_a76):
    case Midr_arm(Part_cortex_a76ae):
    case Midr_arm(Part_cortex_a77):
    case Midr_arm(Part_cortex_a78):
    case Midr_arm(Part_cortex_a78ae):
    case Midr_arm(Part_cortex_a78c):
    case Midr_arm(Part_cortex_a710):
    case Midr_arm(Part_cortex_x1):
    case Midr_arm(Part_cortex_x1c):
    case Midr_arm(Part_cortex_x2):
    case Midr_arm(Part_cortex_x3):
    case Midr_arm(Part_cortex_x4):
    case Midr_arm(Part_cortex_x925):
    case Midr_arm(Part_neoverse_n1):
    case Midr_arm(Part_neoverse_n2):
    case Midr_arm(Part_neoverse_v1):
    case Midr_arm(Part_neoverse_v2):
    case Midr_arm(Part_neoverse_v3):
    case Midr_arm(Part_neoverse_v3ae):
      return true;
    default:
      return false;
    }
}
