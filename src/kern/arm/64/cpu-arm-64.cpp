//----------------------------------------------------------------------
INTERFACE [arm && armv8]:

EXTENSION class Cpu
{
public:
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

    Sctlr_res = (1UL << 11) | (1UL << 20) | (3UL << 22) | (3UL << 28),

    Sctlr_generic = Sctlr_m
                    | (Config::Cp15_c1_use_alignment_check ?  Sctlr_a : 0)
                    | Sctlr_c
                    | Sctlr_cp15ben
                    | Sctlr_sed
                    | Sctlr_i
                    | Sctlr_dze
                    | Sctlr_uct
                    | Sctlr_uci
                    | Sctlr_res,
  };
};

//--------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT_OVERRIDE inline
void
Cpu::init_mmu()
{
  extern char exception_vector[];
  asm volatile ("msr VBAR_EL1, %0" : : "r"(&exception_vector));
}

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
  asm volatile("mrs     %0, SCTLR_E1 \n"
               "orr     %0, %1       \n"
               "msr     SCTLR_E1, %0 \n"
               : : "r" (0), "i" (Sctlr_c | Sctlr_i));
}

PUBLIC static inline
void
Cpu::disable_dcache()
{
  asm volatile("mrs     %0, SCTLR_E1 \n"
               "bic     %0, %1       \n"
               "msr     SCTLR_E1, %0 \n"
               : : "r" (0), "i" (Sctlr_c | Sctlr_i));
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && armv8]:

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

