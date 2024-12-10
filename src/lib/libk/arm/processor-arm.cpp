INTERFACE[arm]:

#include <cxx/bitfield>
#include <cxx/conditionals>

class Arm_esr
{
public:
  Arm_esr() = default;
  explicit Arm_esr(Mword ec) : _raw(ec) {}
  Mword _raw;

  Mword raw() const { return _raw; }

  CXX_BITFIELD_MEMBER(26, 31, ec, _raw);
  CXX_BITFIELD_MEMBER(25, 25, il, _raw);
  CXX_BITFIELD_MEMBER(24, 24, cv, _raw);
  CXX_BITFIELD_MEMBER(20, 23, cond, _raw);

  /// \pre ec == 0x01
  /// Trapped WFE, WFI, WFET or WFIT access.
  CXX_BITFIELD_MEMBER( 0,  0, wfe_trapped, _raw);

  /// \pre ec == 0x03 || ec == 0x4 || ec == 0x5
  /// Trapped MCR, MRC, MCRR or MRRC access.
  CXX_BITFIELD_MEMBER(17, 19, mcr_opc2, _raw);
  CXX_BITFIELD_MEMBER(16, 19, mcrr_opc1, _raw);
  CXX_BITFIELD_MEMBER(14, 16, mcr_opc1, _raw);
  CXX_BITFIELD_MEMBER(10, 13, mcr_crn, _raw);
  CXX_BITFIELD_MEMBER(10, 14, mcrr_rt2, _raw);
  CXX_BITFIELD_MEMBER( 5,  9, mcr_rt, _raw);
  CXX_BITFIELD_MEMBER( 1,  4, mcr_crm, _raw);
  CXX_BITFIELD_MEMBER( 0,  0, mcr_read, _raw);

  Mword mcr_coproc_register() const { return _raw & 0xffc1f; }

  static Mword mcr_coproc_register(unsigned opc1, unsigned crn, unsigned crm, unsigned opc2)
  { return   mcr_opc1_bfm_t::val_dirty(opc1)
           | mcr_crn_bfm_t::val_dirty(crn)
           | mcr_crm_bfm_t::val_dirty(crm)
           | mcr_opc2_bfm_t::val_dirty(opc2); }

  static Mword mrc_coproc_register(unsigned opc1, unsigned crn, unsigned crm, unsigned opc2)
  { return mcr_coproc_register(opc1, crn, crm, opc2) | 1; }

  /// \pre ec == 0x06
  /// Trapped LDC or STC access.
  CXX_BITFIELD_MEMBER(12, 19, ldc_imm, _raw);
  CXX_BITFIELD_MEMBER( 5,  8, ldc_rn, _raw);
  CXX_BITFIELD_MEMBER( 4,  4, ldc_offset_form, _raw);
  CXX_BITFIELD_MEMBER( 1,  3, ldc_addressing_mode, _raw);

  /// \pre ec == 0x07
  /// Trapped SVE, SIMD or FP access.
  CXX_BITFIELD_MEMBER( 5,  5, cpt_simd, _raw);
  CXX_BITFIELD_MEMBER( 0,  3, cpt_cpnr, _raw);

  /// \pre ec == 0x0a
  /// ARMv7 only: Trapped BXJ instruction.
  CXX_BITFIELD_MEMBER( 0,  3, bxj_rm, _raw);

  /// \pre ec == 0x11 || ec == 0x12 || ec == 0x15 || ec == 0x16
  /// Trapped HCV or SVC instruction from Aarch32 or AArch64.
  CXX_BITFIELD_MEMBER( 0, 15, svc_imm, _raw);

  /// \pre ec == 0x20 || ec == 0x21 || ec == 0x24 || ec == 0x25
  /// Instruction abort, Data abort.
  CXX_BITFIELD_MEMBER(24, 24, pf_isv, _raw);
  CXX_BITFIELD_MEMBER(22, 23, pf_sas, _raw);
  CXX_BITFIELD_MEMBER(21, 21, pf_sse, _raw);
  CXX_BITFIELD_MEMBER(16, 19, pf_srt, _raw);
  CXX_BITFIELD_MEMBER(10, 10, pf_fnv, _raw);
  CXX_BITFIELD_MEMBER( 9,  9, pf_ea, _raw);
  CXX_BITFIELD_MEMBER( 8,  8, pf_cache_maint, _raw);
  CXX_BITFIELD_MEMBER( 7,  7, pf_s1ptw, _raw);
  CXX_BITFIELD_MEMBER( 6,  6, pf_write, _raw);
  CXX_BITFIELD_MEMBER( 0,  5, pf_fsc, _raw);

  /// \pre ec == 0x34 || ec == 0x35
  CXX_BITFIELD_MEMBER( 8,  8, wp_cache_maint, _raw);
  CXX_BITFIELD_MEMBER( 6,  6, wp_write, _raw);
  CXX_BITFIELD_MEMBER( 0,  5, wp_dfsc, _raw);

  static Arm_esr make_breakpoint()
  { return Arm_esr((0x30U << 26) | (1U << 25) | 0x22U); }

  static Arm_esr make_watchpoint(Mword cm, Mword wnr)
  {
    Arm_esr esr((0x34U << 26) | (1U << 25));
    esr.wp_cache_maint() = cm;
    esr.wp_write() = wnr;
    esr.wp_dfsc() = 0x22;
    return esr;
  }

  static Arm_esr make_bkpt_insn(Mword il)
  { return Arm_esr((0x38 << 26) | (il << 25)); }

  static Arm_esr make_vector_catch_aarch32()
  { return Arm_esr((0x3aU << 26) | (1U << 25) | 0x22U); }
};

EXTENSION class Proc
{
private:
  enum : unsigned
  {
    Status_FIQ_disabled        = 0x40,
    Status_IRQ_disabled        = 0x80,
  };

public:
  enum : unsigned
  {
    Status_mode_mask           = 0x1f,

    Status_interrupts_disabled = Status_FIQ_disabled | Status_IRQ_disabled,
    Status_thumb               = 0x20,

    PSR_m_usr = 0x10,
    PSR_m_fiq = 0x11,
    PSR_m_irq = 0x12,
    PSR_m_svc = 0x13,
    PSR_m_mon = 0x16,
    PSR_m_abt = 0x17,
    PSR_m_hyp = 0x1a,
    PSR_m_und = 0x1b,
    PSR_m_sys = 0x1f,

    Is_hyp = cxx::const_ite<TAG_ENABLED(cpu_virt)>(1, 0),
    Status_mode_supervisor = cxx::const_ite<Is_hyp>(PSR_m_hyp, PSR_m_svc),
  };

  static Cpu_phys_id cpu_id();
};

//--------------------------------------------------------------------
INTERFACE[arm && !arm_em_tz]:

#define ARM_CPS_INTERRUPT_FLAGS "if"

EXTENSION class Proc
{
public:
  enum : unsigned
    {
      Cli_mask                = Status_interrupts_disabled,
      Sti_mask                = Status_interrupts_disabled,
      Status_preempt_disabled = Status_IRQ_disabled,
      Status_interrupts_mask  = Status_interrupts_disabled,
      Status_always_mask      = Status_mode_always_on,
    };
};

//--------------------------------------------------------------------
INTERFACE[arm && arm_em_tz]:

#define ARM_CPS_INTERRUPT_FLAGS "f"

EXTENSION class Proc
{
public:
  enum : unsigned
    {
      Cli_mask                = Status_FIQ_disabled,
      Sti_mask                = Status_FIQ_disabled,
      Status_preempt_disabled = Status_FIQ_disabled,
      Status_interrupts_mask  = Status_FIQ_disabled,
      Status_always_mask      = Status_mode_always_on | Status_IRQ_disabled,
    };
};

//--------------------------------------------------------------------
IMPLEMENTATION[arm]:

#include "types.h"
#include "std_macros.h"

IMPLEMENT static inline
Mword Proc::stack_pointer()
{
  Mword sp;
  asm volatile ("mov %0, sp" : "=r" (sp));
  return sp;
}

IMPLEMENT static inline
void Proc::stack_pointer(Mword sp)
{
  asm volatile ("mov sp, %0" : : "r" (sp));
}

IMPLEMENT static inline ALWAYS_INLINE
void Proc::sti_restore(Status st)
{
  if (!(st & Sti_mask))
    sti();
}

IMPLEMENT static inline
void Proc::irq_chance()
{
  asm volatile ("nop; nop" : : : "memory");
}

//----------------------------------------------------------------
IMPLEMENTATION[arm && (arm_pxa || arm_sa || arm_920t)]:

IMPLEMENT static inline
void Proc::halt()
{}

IMPLEMENT static inline
void Proc::pause()
{}

//----------------------------------------------------------------
IMPLEMENTATION[arm_v7 || arm_v8]:

IMPLEMENT static inline
void Proc::pause()
{
  asm("yield");
}

IMPLEMENT static inline
void Proc::halt()
{
  Status f = cli_save();
  asm volatile("dsb sy \n\t"
               "isb sy \n\t"
               "wfi \n\t");
  sti_restore(f);
}
