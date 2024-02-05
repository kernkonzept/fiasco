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

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit && mmu]:

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    Hcr_must_set_bits = Hcr_vm | Hcr_swio | Hcr_ptw
                      | Hcr_amo | Hcr_imo | Hcr_fmo
                      | Hcr_tidcp | Hcr_tsc | Hcr_tactlr,
  };
};

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  asm volatile (
        "mcr p15, 4, %0, c2, c1, 2" // VTCR
        : : "r" ((1UL << 31) | (Page::Tcr_attribs << 8) | (1 << 6)));
  init_hyp_mode_common();
}

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit && !mmu]:

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    Hcr_must_set_bits = Hcr_vm | Hcr_swio
                      | Hcr_amo| Hcr_imo | Hcr_fmo
                      | Hcr_tidcp | Hcr_tsc | Hcr_tactlr,
  };
};

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  init_hyp_mode_common();
}

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit]:

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    /**
     * HCR value to be used for the VMM.
     *
     * The VMM runs in system mode (PL1), but has extended
     * CP15 access allowed.
     */
    Hcr_host_bits = Hcr_must_set_bits | Hcr_dc,

    /**
     * HCR value to be used for normal threads.
     *
     * On a hyp kernel they can choose to run in PL1 or PL0.
     * However, all but the TPIDxyz CP15 accesses are disabled.
     */
    Hcr_non_vm_bits_common = Hcr_must_set_bits | Hcr_dc | Hcr_tsw
                             | Hcr_ttlb | Hcr_tvm,
    Hcr_non_vm_bits_el1    = Hcr_non_vm_bits_common,
    Hcr_non_vm_bits_el0    = Hcr_non_vm_bits_common | Hcr_tge,
  };

  enum
  {
    Hdcr_bits = Mdcr_tpmcr | Mdcr_tpm   | Mdcr_tde
                | Mdcr_tda | Mdcr_tdosa | Mdcr_tdra,
  };
};

PRIVATE inline
void
Cpu::init_hyp_mode_common()
{
  extern char hyp_vector_base[];

  assert (!(reinterpret_cast<Mword>(hyp_vector_base) & 31));
  asm volatile ("mcr p15, 4, %0, c12, c0, 0 \n" : : "r"(hyp_vector_base));

  asm volatile (
        "mcr p15, 4, %0, c1, c1, 1 \n"

        "mrc p15, 0, r0, c1, c0, 0 \n"
        "bic r0, #1 \n"
        "mcr p15, 0, r0, c1, c0, 0 \n"

        "mcr p15, 4, %1, c1, c1, 0 \n"
        : :
        "r" (Mword{Hdcr_bits} | (has_hpmn0() ? 0 : 1)),
        "r" (Hcr_non_vm_bits_el0)
        : "r0" );

  asm ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Hstr_non_vm)); // HSTR

  Mem::dsb();
  Mem::isb();

  // HCPTR
  asm volatile("mcr p15, 4, %0, c1, c1, 2" : :
               "r" (  0x33ff       // TCP: 0-9, 12-13
                    | (1 << 20))); // TTA
}
