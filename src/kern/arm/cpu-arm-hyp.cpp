IMPLEMENTATION [arm && cpu_virt]:

#include "feature.h"

KIP_KERNEL_FEATURE("arm:hyp");

EXTENSION class Cpu
{
public:
  enum
  {
    // HDCR[31:0] (arm32) is architecturally mapped to MDCR_EL2[31:0] (arm64).
    Mdcr_hpmn_mask = 0xf,
    Mdcr_tpmcr     = 1UL << 5,
    Mdcr_tpm       = 1UL << 6,
    Mdcr_hpme      = 1UL << 7,
    Mdcr_tde       = 1UL << 8,
    Mdcr_tda       = 1UL << 9,
    Mdcr_tdosa     = 1UL << 10,
    Mdcr_tdra      = 1UL << 11,
  };
};

//--------------------------------------------------------
INTERFACE [arm && cpu_virt && arm_v7]:

EXTENSION class Cpu
{
public:
  enum Hstr_values
  {
    Hstr_non_vm = 0x39f6f, // ALL but crn=13,7 (TPIDxxR, DSB) CP15 traped
    Hstr_vm = 0x0, // none
  };

};

//--------------------------------------------------------
INTERFACE [arm && cpu_virt && arm_v8]:

EXTENSION class Cpu
{
public:
  enum Hstr_values
  {
    Hstr_non_vm = 0x9f6f, // ALL but crn=13,7 (TPIDxxR, DSB) CP15 traped
    Hstr_vm = 0x0, // none
  };
};
