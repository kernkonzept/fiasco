INTERFACE [arm]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_cache_enabled  = Cp15_c1_generic | Cp15_c1_cache_bits,
    Cp15_c1_cache_disabled = Cp15_c1_generic,
  };

  static Unsigned32 sctlr;
  bool has_generic_timer() const { return (_cpu_id._pfr[1] & 0xf0000) == 0x10000; }
};

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

Unsigned32 Cpu::sctlr;

PRIVATE static inline
Mword
Cpu::midr()
{
  Mword m;
  asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
  return m;
}

PRIVATE static inline
void
Cpu::check_for_swp_enable()
{
  if (!Config::Cp15_c1_use_swp_enable)
    return;

  Mword midr;
  asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
  if (((midr >> 16) & 0xf) != 0xf)
    return; // pre ARMv7 has no swap enable / disable

  Mword mpidr, id_isar0;
  asm volatile ("mrc p15, 0, %0, c0, c2, 0" : "=r"(id_isar0));
  asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
  if ((id_isar0 & 0xf) != 1)
    return; // CPU has no swp / swpb

  if (((mpidr >> 31) & 1) == 0)
    return; // CPU has no MP extensions -> no swp enable

  sctlr |= Cp15_c1_v7_sw;
}

IMPLEMENT
void Cpu::early_init()
{
  sctlr = Config::Cache_enabled
          ? Cp15_c1_cache_enabled : Cp15_c1_cache_disabled;

  check_for_swp_enable();

  // switch to supervisor mode and intialize the memory system
  asm volatile ( " mov  r2, r13             \n"
                 " mov  r3, r14             \n"
                 " msr  cpsr_c, %1          \n"
                 " mov  r13, r2             \n"
                 " mov  r14, r3             \n"

                 " mcr  p15, 0, %0, c1, c0  \n"
                 :
                 : "r" (sctlr),
                   "I" (Proc::Status_mode_supervisor
                        | Proc::Status_interrupts_disabled)
                 : "r2", "r3");

  early_init_platform();

  Mem_unit::flush_cache();
}

PUBLIC static inline
void
Cpu::enable_dcache()
{
  asm volatile("mrc     p15, 0, %0, c1, c0, 0 \n"
               "orr     %0, %1                \n"
               "mcr     p15, 0, %0, c1, c0, 0 \n"
               : : "r" (0), "i" (Cp15_c1_cache));
}

PUBLIC static inline
void
Cpu::disable_dcache()
{
  asm volatile("mrc     p15, 0, %0, c1, c0, 0 \n"
               "bic     %0, %1                \n"
               "mcr     p15, 0, %0, c1, c0, 0 \n"
               : : "r" (0), "i" (Cp15_c1_cache));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

PRIVATE static inline
void
Cpu::modify_actrl(Mword set_mask, Mword clear_mask)
{
  Mword t;
  asm volatile("mrc p15, 0, %[reg], c1, c0, 1 \n\t"
               "bic %[reg], %[reg], %[clr]    \n\t"
               "orr %[reg], %[reg], %[set]    \n\t"
               "mcr p15, 0, %[reg], c1, c0, 1 \n\t"
               : [reg] "=r" (t)
               : [set] "r" (set_mask), [clr] "r" (clear_mask));
}

PRIVATE static inline NEEDS[Cpu::modify_actrl]
void
Cpu::set_actrl(Mword bit_mask)
{ modify_actrl(bit_mask, 0); }

PRIVATE static inline NEEDS[Cpu::modify_actrl]
void
Cpu::clear_actrl(Mword bit_mask)
{ modify_actrl(0, bit_mask); }

IMPLEMENT
void
Cpu::id_init()
{
  __asm__("mrc p15, 0, %0, c0, c1, 0": "=r" (_cpu_id._pfr[0]));
  __asm__("mrc p15, 0, %0, c0, c1, 1": "=r" (_cpu_id._pfr[1]));
  __asm__("mrc p15, 0, %0, c0, c1, 2": "=r" (_cpu_id._dfr0));
  __asm__("mrc p15, 0, %0, c0, c1, 3": "=r" (_cpu_id._afr0));
  __asm__("mrc p15, 0, %0, c0, c1, 4": "=r" (_cpu_id._mmfr[0]));
  __asm__("mrc p15, 0, %0, c0, c1, 5": "=r" (_cpu_id._mmfr[1]));
  __asm__("mrc p15, 0, %0, c0, c1, 6": "=r" (_cpu_id._mmfr[2]));
  __asm__("mrc p15, 0, %0, c0, c1, 7": "=r" (_cpu_id._mmfr[3]));
}

