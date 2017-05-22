//----------------------------------------------------------------------
INTERFACE [arm && arm_v8]:

EXTENSION class Cpu
{
public:
  enum Scr_bits
  {
    Scr_ns  = 1UL <<  0, ///< Non-Secure mode
    Scr_irq = 1UL <<  1, ///< IRQ to EL3
    Scr_fiq = 1UL <<  2, ///< FIQ to EL3
    Scr_ea  = 1UL <<  3, ///< External Abort and SError to EL3
    Scr_smd = 1UL <<  7, ///< SMC disable
    Scr_hce = 1UL <<  8, ///< HVC enable at EL1, EL2, and EL3
    Scr_sif = 1UL <<  9, ///< Secure instruction fetch enable
    Scr_rw  = 1UL << 10, ///< EL2 / EL1 is AArch64
    Scr_st  = 1UL << 11, ///< Trap Secure EL1 access to timer to EL3
    Scr_twi = 1UL << 12, ///< Trap WFI to EL3
    Scr_twe = 1UL << 13, ///< Trap WFE to EL3
  };

  enum {
    Sctlr_m       = 1UL << 0,
    Sctlr_a       = 1UL << 1,
    Sctlr_c       = 1UL << 2,
    Sctlr_sa      = 1UL << 3,
    Sctlr_sa0     = 1UL << 4,
    Sctlr_cp15ben = 1UL << 5,
    Sctlr_itd     = 1UL << 7,
    Sctlr_sed     = 1UL << 8,
    Sctlr_uma     = 1UL << 9,
    Sctlr_i       = 1UL << 12,
    Sctlr_dze     = 1UL << 14,
    Sctlr_uct     = 1UL << 15,
    Sctlr_ntwi    = 1UL << 16,
    Sctlr_ntwe    = 1UL << 18,
    Sctlr_wxn     = 1UL << 19,
    Sctlr_e0e     = 1UL << 24,
    Sctlr_ee      = 1UL << 25,
    Sctlr_uci     = 1UL << 26,

    Sctlr_el1_res = (1UL << 11) | (1UL << 20) | (3UL << 22) | (3UL << 28),

    Sctlr_el1_generic = Sctlr_c
                    | Sctlr_cp15ben
                    | Sctlr_i
                    | Sctlr_dze
                    | Sctlr_uct
                    | Sctlr_uci
                    | Sctlr_el1_res,
  };
};

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

EXTENSION class Cpu
{
public:
  enum
  {
    Sctlr_generic = Sctlr_el1_generic
                    | Sctlr_m
                    | (Config::Cp15_c1_use_alignment_check ?  Sctlr_a : 0)
  };
};

IMPLEMENT_OVERRIDE inline
void
Cpu::init_mmu(bool)
{
  extern char exception_vector[];
  asm volatile ("msr VBAR_EL1, %0" : : "r"(&exception_vector));
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

EXTENSION class Cpu
{
public:
  enum : Unsigned64
  {
    Hcr_rw = 1UL << 31,
    Hcr_must_set_bits = Hcr_vm | Hcr_swio | Hcr_ptw
                      | Hcr_amo | Hcr_imo | Hcr_fmo
                      | Hcr_tidcp | Hcr_tsc | Hcr_tactlr,

    Hcr_host_bits = Hcr_must_set_bits | Hcr_rw | Hcr_dc | Hcr_tge
  };

  enum
  {
    Sctlr_res = (3UL << 4)  | (1UL << 11) | (1UL << 16)
              | (1UL << 18) | (3UL << 22) | (3UL << 28),

    Sctlr_generic = Sctlr_m
                    | (Config::Cp15_c1_use_alignment_check ?  Sctlr_a : 0)
                    | Sctlr_c
                    | Sctlr_i
                    | Sctlr_res,
  };

  enum {
    Mdcr_hpmn_mask = 0xf,
    Mdcr_tpmcr     = 1UL << 5,
    Mdcr_tpm       = 1UL << 6,
    Mdcr_hpme      = 1UL << 7,
    Mdcr_tde       = 1UL << 8,
    Mdcr_tda       = 1UL << 9,
    Mdcr_tdosa     = 1UL << 10,
    Mdcr_tdra      = 1UL << 11,

    Mdcr_bits      = Mdcr_tpmcr | Mdcr_tpm,
    Mdcr_vm_mask   = 0xf00,
  };
};

IMPLEMENT_OVERRIDE inline void Cpu::init_mmu(bool) {}

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  extern char exception_vector[];
  asm volatile ("msr VBAR_EL2, %0" : : "r"(&exception_vector));
  asm volatile ("msr VTCR_EL2, %0" : :
                "r"(  (1UL << 31) // RES1
                    | (Page::Tcr_attribs << 8)
                    // sl = 1       PS = 40bit   t0sz = 40bit
                    | (1UL << 6) | (2UL << 16) | 25));

  asm volatile (
        "msr MDCR_EL2, %0"
        : : "r"(Mdcr_bits));

  asm volatile ("msr SCTLR_EL1, %0" : : "r"(Sctlr_el1_generic));
  asm volatile ("msr HCR_EL2, %0" : : "r" (Hcr_host_bits));

  Mem::dsb();
  Mem::isb();

  // HCPTR
  asm volatile("msr CPTR_EL2, %0" : :
               "r" (  0x33ff       // TCP: 0-9, 12-13
                    | (1 << 20))); // TTA
}
//--------------------------------------------------------------------------
IMPLEMENTATION [arm]:

PUBLIC static inline
bool
Cpu::has_generic_timer()
{ return true; }

PRIVATE static inline
Mword
Cpu::midr()
{
  Mword m;
  asm volatile ("mrs %0, midr_el1" : "=r" (m));
  return m;
}

IMPLEMENT
void
Cpu::id_init()
{
  __asm__("mrs %0, ID_AA64PFR0_EL1": "=r" (_cpu_id._pfr[0]));
  __asm__("mrs %0, ID_AA64PFR1_EL1": "=r" (_cpu_id._pfr[1]));
  __asm__("mrs %0, ID_AA64DFR0_EL1": "=r" (_cpu_id._dfr0));
  __asm__("mrs %0, ID_AA64DFR1_EL1": "=r" (_cpu_id._afr0));
  __asm__("mrs %0, ID_AA64MMFR0_EL1": "=r" (_cpu_id._mmfr[0]));
  __asm__("mrs %0, ID_AA64MMFR1_EL1": "=r" (_cpu_id._mmfr[1]));
}

IMPLEMENT
void Cpu::early_init()
{
  early_init_platform();

  Mem_unit::flush_cache();
}

PUBLIC static inline
void
Cpu::enable_dcache()
{
  Mword r;
  asm volatile("mrs     %0, SCTLR_EL1 \n"
               "orr     %0, %0, %1    \n"
               "msr     SCTLR_EL1, %0 \n"
               : "=&r" (r) : "r" (Sctlr_c | Sctlr_i));
}

PUBLIC static inline
void
Cpu::disable_dcache()
{
  Mword r;
  asm volatile("mrs     %0, SCTLR_EL1 \n"
               "bic     %0, %0, %1    \n"
               "msr     SCTLR_EL1, %0 \n"
               : "=&r" (r) : "r" (Sctlr_c | Sctlr_i));
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

PUBLIC static inline NEEDS[Cpu::midr]
bool
Cpu::is_smp_capable()
{
  // Check for ARM Cortex A53+ CPUs
  Mword id = midr();
  return (id & 0xff0fff00) == 0x410fd000;
}

PUBLIC static inline
void
Cpu::enable_smp()
{
  if (!is_smp_capable())
    return;

  Mword cpuectl;
  asm volatile ("mrs %0, S3_1_C15_C2_1" : "=r" (cpuectl));
  if (!(cpuectl & (1 << 6)))
    asm volatile ("msr S3_1_C15_C2_1, %0" : : "r" (cpuectl | (1 << 6)));
}

PUBLIC static inline
void
Cpu::disable_smp()
{
  if (!is_smp_capable())
    return;

  Mword cpuectl;
  asm volatile ("mrs %0, S3_1_C15_C2_1" : "=r" (cpuectl));
  if (cpuectl & (1 << 6))
    asm volatile ("msr S3_1_C15_C2_1, %0" : : "r" (cpuectl & ~(1UL << 6)));
}

