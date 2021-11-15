IMPLEMENTATION [arm && cpu_virt]:

#include "feature.h"

EXTENSION class Cpu
{
public:
  enum : Mword
  {
    Hcr_vm     = 1UL << 0,  ///< Virtualization enable
    Hcr_swio   = 1UL << 1,  ///< Set/way invalidation override
    Hcr_ptw    = 1UL << 2,  ///< Protected table walk
    Hcr_fmo    = 1UL << 3,  ///< Physical FIQ routing
    Hcr_imo    = 1UL << 4,  ///< Physical IRQ routing
    Hcr_amo    = 1UL << 5,  ///< Physical SError interrupt routing
    Hcr_dc     = 1UL << 12, ///< Default cacheability
    Hcr_tid2   = 1UL << 17, ///< Trap CTR, CESSLR, etc.
    Hcr_tid3   = 1UL << 18, ///< Trap ID, etc.
    Hcr_tsc    = 1UL << 19, ///< Trap SMC instructions
    Hcr_tidcp  = 1UL << 20, ///< Trap implementation defined functionality
    Hcr_tactlr = 1UL << 21, ///< Trap ACTLR, etc.
    Hcr_tsw    = 1UL << 22, ///< Trap cache maintenance instructions
    Hcr_ttlb   = 1UL << 25, ///< Trap TLB maintenance instructions
    Hcr_tvm    = 1UL << 26, ///< Trap virtual memory controls
    Hcr_tge    = 1UL << 27, ///< Trap General Exceptions
    Hcr_hcd    = 1UL << 29, ///< HVC instruction disable
    Hcr_trvm   = 1UL << 30, ///< Trap reads of virtual memory controls
    Hcr_rw     = 1UL << 31, ///< EL1 is AArch64
  };
};

KIP_KERNEL_FEATURE("arm:hyp");

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
IMPLEMENTATION [arm && cpu_virt && 32bit]:

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    Hcr_must_set_bits = Hcr_vm | Hcr_swio | Hcr_ptw
                      | Hcr_amo | Hcr_imo | Hcr_fmo
                      | Hcr_tidcp | Hcr_tsc | Hcr_tactlr,

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
     * On a hyp kernel all threads run per default in system mode (PL1).
     * However, all but the TPIDxyz CP15 accesses are disabled.
     */
    Hcr_non_vm_bits = Hcr_must_set_bits | Hcr_dc | Hcr_tsw
                      | Hcr_ttlb | Hcr_tvm
  };
};

IMPLEMENT_OVERRIDE inline void Cpu::init_mmu(bool) {}

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  extern char hyp_vector_base[];

  assert (!((Mword)hyp_vector_base & 31));
  asm volatile ("mcr p15, 4, %0, c12, c0, 0 \n" : : "r"(hyp_vector_base));

  asm volatile (
        "mrc p15, 4, r0, c1, c1, 1 \n"
        "orr r0, #(0xf << 8) \n" // enable TDE, TDA, TDOSA, TDRA
        "mcr p15, 4, r0, c1, c1, 1 \n"

        "mcr p15, 4, %0, c2, c1, 2 \n"

        "mrc p15, 0, r0, c1, c0, 0 \n"
        "bic r0, #1 \n"
        "mcr p15, 0, r0, c1, c0, 0 \n"

        "mcr p15, 4, %1, c1, c1, 0 \n"
        : :
        "r" ((1UL << 31) | (Page::Tcr_attribs << 8) | (1 << 6)),
        "r" (Hcr_non_vm_bits)
        : "r0" );

  asm ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Hstr_non_vm)); // HSTR

  Mem::dsb();
  Mem::isb();

  // HCPTR
  asm volatile("mcr p15, 4, %0, c1, c1, 2" : :
               "r" (  0x33ff       // TCP: 0-9, 12-13
                    | (1 << 20))); // TTA
}
