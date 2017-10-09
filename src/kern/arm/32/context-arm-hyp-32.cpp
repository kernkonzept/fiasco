INTERFACE [arm && cpu_virt]:

#include "vgic.h"

EXTENSION class Context
{
public:
  struct Vm_state
  {
    struct Per_mode_regs
    {
      Mword sp;
      Mword lr;
      Mword spsr;
    };

    struct Regs
    {
      Unsigned32 hcr;

      Unsigned64 ttbr0;
      Unsigned64 ttbr1;
      Unsigned32 ttbcr;
      Unsigned32 sctlr;
      Unsigned32 dacr;
      Unsigned32 fcseidr;
      Unsigned32 contextidr;
      Unsigned32 cntkctl;
    };

    typedef Arm_vgic_t<4> Gic;

    /* The following part is our user API */
    Regs guest_regs;
    Regs host_regs;
    Gic  gic;

    Unsigned64 cntvoff;

    Unsigned64 cntv_cval;
    Unsigned32 cntkctl;
    Unsigned32 cntv_ctl;

    Unsigned32 vmpidr;

    /* The user API ends here */

    /* we should align this at a cache line ... */
    Unsigned32 hcr;

    Unsigned32 csselr;

    Unsigned32 sctlr;
    Unsigned32 actlr;
    Unsigned32 cpacr;

    Unsigned64 ttbr0;
    Unsigned64 ttbr1;
    Unsigned32 ttbcr;

    Unsigned32 dacr;

    Unsigned32 dfsr;
    Unsigned32 ifsr;
    Unsigned32 adfsr;
    Unsigned32 aifsr;

    Unsigned32 dfar;
    Unsigned32 ifar;

    Unsigned64 par;

    Unsigned32 mair0;
    Unsigned32 mair1;

    Unsigned32 amair0;
    Unsigned32 amair1;

    Unsigned32 vbar;

    Unsigned32 fcseidr;
    Unsigned32 contextidr;
    Unsigned32 tpidrprw;

    Per_mode_regs svc;
    Per_mode_regs abt;
    Per_mode_regs und;
    Per_mode_regs irq;
    Mword fiq_r8;
    Mword fiq_r9;
    Mword fiq_r10;
    Mword fiq_r11;
    Mword fiq_r12;
    Per_mode_regs fiq;

    Unsigned32 fpinst;
    Unsigned32 fpinst2;
  };
};


//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "mem.h"

IMPLEMENT inline
void
Context::fill_user_state()
{}

IMPLEMENT inline
void
Context::spill_user_state()
{}


IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  Unsigned32 const forbidden = (state() & Thread_ext_vcpu_enabled)
                               ? ~0x888f0000U  // allow all but hyp mode
                               : ~0x00010000U; // only usermode allowed (0x10)

  if ((1UL << (dst->psr & Proc::Status_mode_mask)) & forbidden)
    dst->psr = (dst->psr & ~Proc::Status_mode_mask) | Proc::PSR_m_usr;
}

IMPLEMENT_OVERRIDE inline NEEDS["mem.h", Context::sanitize_user_state]
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{
  Mem::memcpy_mwords(dst, src, 18);
  dst->pc = src->pc;
  dst->psr = access_once(&src->psr);
  sanitize_user_state(dst);
}

PRIVATE inline
void
Context::arm_hyp_load_non_vm_state(bool vgic)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 0"
                : : "r"(Cpu::Hcr_tge | Cpu::Hcr_dc | Cpu::Hcr_must_set_bits));
  // load normal SCTLR ...
  asm volatile ("mcr p15, 0, %0, c1, c0, 0"
                : : "r" ((Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~Cpu::Cp15_c1_mmu));
  asm volatile ("mcr p15, 0, %0,  c1, c0, 2" : : "r" (0xf00000));
  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r" (0));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r" (0));
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (0)); // disable VTIMER
  // CNTKCTL: allow access to virtual and physical counter from PL0
  // see: generic_timer.cpp: setup_timer_access (Hyp)
  asm volatile("mcr p15, 0, %0, c14, c1, 0" : : "r"(0x3));

  if (vgic)
    Gic_h::gic->hcr(Gic_h::Hcr(0));
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Mword _state, Vm_state *v)
{
  // save vm state
  asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(_tpidruro));

  asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(v->hcr));
  asm volatile ("mrc p15, 2, %0, c0, c0, 0" : "=r"(v->csselr));

  asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(v->sctlr));
  // we unconditionally trap actlr accesses
  // asm ("mrc p15, 0, %0, c1, c0, 1" : "=r"(v->actlr));
  asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r"(v->cpacr));

  asm volatile ("mrrc p15, 0, %Q0, %R0, c2" : "=r"(v->ttbr0));
  asm volatile ("mrrc p15, 1, %Q0, %R0, c2" : "=r"(v->ttbr1));
  asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r"(v->ttbcr));

  asm volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r"(v->dacr));

  asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r"(v->dfsr));
  asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r"(v->ifsr));
  asm volatile ("mrc p15, 0, %0, c5, c1, 0" : "=r"(v->adfsr));
  asm volatile ("mrc p15, 0, %0, c5, c1, 1" : "=r"(v->aifsr));

  asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r"(v->dfar));
  asm volatile ("mrc p15, 0, %0, c6, c0, 2" : "=r"(v->ifar));

  asm volatile ("mrrc p15, 0, %Q0, %R0, c7" : "=r"(v->par));

  asm volatile ("mrc p15, 0, %0, c10, c2, 0" : "=r"(v->mair0));
  asm volatile ("mrc p15, 0, %0, c10, c2, 1" : "=r"(v->mair1));

  asm volatile ("mrc p15, 0, %0, c10, c3, 0" : "=r"(v->amair0));
  asm volatile ("mrc p15, 0, %0, c10, c3, 1" : "=r"(v->amair1));

  asm volatile ("mrc p15, 0, %0, c12, c0, 0" : "=r"(v->vbar));

  asm volatile ("mrc p15, 0, %0, c13, c0, 0" : "=r"(v->fcseidr));
  asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r"(v->contextidr));
  asm volatile ("mrc p15, 0, %0, c13, c0, 4" : "=r"(v->tpidrprw));

#define SAVE_GP_MODE(m) \
  asm volatile ("mrs %0, SP_"#m : "=r"(v->m.sp)); \
  asm volatile ("mrs %0, LR_"#m : "=r"(v->m.lr)); \
  asm volatile ("mrs %0, SPSR_"#m : "=r"(v->m.spsr))

  SAVE_GP_MODE(svc);
  SAVE_GP_MODE(abt);
  SAVE_GP_MODE(und);
  SAVE_GP_MODE(irq);
  asm volatile ("mrs %0, R8_fiq" : "=r"(v->fiq_r8));
  asm volatile ("mrs %0, R9_fiq" : "=r"(v->fiq_r9));
  asm volatile ("mrs %0, R10_fiq" : "=r"(v->fiq_r10));
  asm volatile ("mrs %0, R11_fiq" : "=r"(v->fiq_r11));
  asm volatile ("mrs %0, R12_fiq" : "=r"(v->fiq_r12));
  SAVE_GP_MODE(fiq);
#undef SAVE_GP_MODE

  asm volatile ("mrrc p15, 3, %Q0, %R0, c14" : "=r" (v->cntv_cval));
  asm volatile ("mrrc p15, 4, %Q0, %R0, c14" : "=r" (v->cntvoff));
  asm volatile ("mrc p15, 0, %0, c14, c1, 0" : "=r" (v->cntkctl));
  if ((_state & Thread_vcpu_user))
    asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (v->cntv_ctl));
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Mword _to_state, Vm_state const *v)
{
  Unsigned32 hcr = access_once(&v->hcr) | Cpu::Hcr_must_set_bits;
  asm volatile ("mcr p15, 4, %0, c1, c1, 0" : : "r"(hcr));
  asm volatile ("mcr p15, 2, %0, c0, c0, 0" : : "r"(v->csselr));

  Unsigned32 sctlr = access_once(&v->sctlr);
  if (hcr & Cpu::Hcr_tge)
    sctlr &= ~Cpu::Cp15_c1_mmu;

  asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr));
  // we unconditionally trap actlr accesses
  // asm ("mcr p15, 0, %0, c1, c0, 1" : : "r"(v->actlr));
  asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r"(v->cpacr));

  asm volatile ("mcrr p15, 0, %Q0, %R0, c2" : : "r"(v->ttbr0));
  asm volatile ("mcrr p15, 1, %Q0, %R0, c2" : : "r"(v->ttbr1));
  asm volatile ("mcr p15, 0, %0, c2, c0, 2" : : "r"(v->ttbcr));

  asm volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r"(v->dacr));

  asm volatile ("mcr p15, 0, %0, c5, c0, 0" : : "r"(v->dfsr));
  asm volatile ("mcr p15, 0, %0, c5, c0, 1" : : "r"(v->ifsr));
  asm volatile ("mcr p15, 0, %0, c5, c1, 0" : : "r"(v->adfsr));
  asm volatile ("mcr p15, 0, %0, c5, c1, 1" : : "r"(v->aifsr));

  asm volatile ("mcr p15, 0, %0, c6, c0, 0" : : "r"(v->dfar));
  asm volatile ("mcr p15, 0, %0, c6, c0, 2" : : "r"(v->ifar));

  asm volatile ("mcrr p15, 0, %Q0, %R0, c7" : : "r"(v->par));

  asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(v->mair0));
  asm volatile ("mcr p15, 0, %0, c10, c2, 1" : : "r"(v->mair1));

  asm volatile ("mcr p15, 0, %0, c10, c3, 0" : : "r"(v->amair0));
  asm volatile ("mcr p15, 0, %0, c10, c3, 1" : : "r"(v->amair1));

  asm volatile ("mcr p15, 0, %0, c12, c0, 0" : : "r"(v->vbar));

  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(v->fcseidr));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r"(v->contextidr));
  asm volatile ("mcr p15, 0, %0, c13, c0, 4" : : "r"(v->tpidrprw));

#define LOAD_GP_MODE(m) \
  asm volatile ("msr SP_"#m ", %0" : : "r"(v->m.sp)); \
  asm volatile ("msr LR_"#m ", %0" : : "r"(v->m.lr)); \
  asm volatile ("msr SPSR_"#m ", %0" : : "r"(v->m.spsr))

  LOAD_GP_MODE(svc);
  LOAD_GP_MODE(abt);
  LOAD_GP_MODE(und);
  LOAD_GP_MODE(irq);
  asm volatile ("msr R8_fiq,  %0" : : "r"(v->fiq_r8));
  asm volatile ("msr R9_fiq,  %0" : : "r"(v->fiq_r9));
  asm volatile ("msr R10_fiq, %0" : : "r"(v->fiq_r10));
  asm volatile ("msr R11_fiq, %0" : : "r"(v->fiq_r11));
  asm volatile ("msr R12_fiq, %0" : : "r"(v->fiq_r12));
  LOAD_GP_MODE(fiq);
#undef LOAD_GP_MODE

  asm volatile ("mcrr p15, 3, %Q0, %R0, c14" : : "r" (v->cntv_cval));
  asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r" (v->cntvoff));
  asm volatile ("mcr p15, 0, %0, c14, c1, 0" : : "r" (v->cntkctl));

  if ((_to_state & Thread_vcpu_user))
    asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (v->cntv_ctl));
  else
    asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r"(0));

  asm volatile ("mcr  p15, 4, %0, c0, c0, 5" : : "r" (v->vmpidr));
}

PRIVATE static inline
Mword
Context::arm_get_hcr()
{
  Mword v;
  asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(v));
  return v;
}

PRIVATE static inline
Unsigned32 Context::arm_host_sctlr()
{
  return (Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~(Cpu::Cp15_c1_mmu | (1 << 28));
}

PRIVATE inline NEEDS[Context::arm_host_sctlr]
void
Context::arm_ext_vcpu_switch_to_host(Vcpu_state *vcpu, Vm_state *v)
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 3"
                : "=r"(vcpu->_regs.tpidruro));
  asm volatile ("mrc p15, 0, %0, c1,  c0, 0"
                : "=r"(v->guest_regs.sctlr));
  asm volatile ("mrc p15, 0, %0, c13, c0, 0"
                : "=r"(v->guest_regs.fcseidr));
  asm volatile ("mrc p15, 0, %0, c13, c0, 1"
                : "=r"(v->guest_regs.contextidr));
  asm volatile ("mrc p15, 0, %0, c14, c1, 0"
                : "=r" (v->guest_regs.cntkctl));

  // fcse not supported in vmm
  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(0));
  asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r"(arm_host_sctlr()));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1"
                : : "r"(v->host_regs.contextidr));
  asm volatile ("mcr p15, 0, %0, c14, c1, 0" : : "r"(Host_cntkctl));

  asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (v->cntv_ctl));
  // disable VTIMER
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r"(0));
}

PRIVATE inline NEEDS[Context::arm_host_sctlr]
void
Context::arm_ext_vcpu_switch_to_host_no_load(Vcpu_state *vcpu, Vm_state *v)
{
  vcpu->_regs.tpidruro     = _tpidruro;
  v->guest_regs.sctlr      = v->sctlr;
  v->guest_regs.fcseidr    = v->fcseidr;
  v->guest_regs.contextidr = v->contextidr;
  v->guest_regs.cntkctl    = v->cntkctl;

  v->sctlr      = arm_host_sctlr();
  v->cntkctl    = Host_cntkctl;
  v->fcseidr    = 0;
  v->contextidr = v->host_regs.contextidr;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_host_regs(Vcpu_state *vcpu, Vm_state *)
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->host.tpidruro));
  asm volatile ("mcr p15, 4, %0, c1,  c1, 0" : : "r"(Cpu::Hcr_host_bits));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest(Vcpu_state *, Vm_state *v)
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 1"
                : "=r"(v->host_regs.contextidr));

  asm volatile ("mcr p15, 0, %0, c13, c0, 0"
                : : "r"(v->guest_regs.fcseidr));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1"
                : : "r"(v->guest_regs.contextidr));

  asm volatile ("mcr p15, 0, %0, c14, c1, 0" : : "r" (v->guest_regs.cntkctl));
  asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r"(v->guest_regs.sctlr));
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (v->cntv_ctl));

  asm volatile ("mcr  p15, 4, %0, c0, c0, 5" : : "r" (v->vmpidr));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest_no_load(Vcpu_state *, Vm_state *v)
{
  v->fcseidr    = v->guest_regs.fcseidr;
  v->contextidr = v->guest_regs.contextidr;
  v->cntkctl    = v->guest_regs.cntkctl;
  v->sctlr      = v->guest_regs.sctlr;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *, Mword hcr)
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(vcpu->host.tpidruro));
  asm volatile ("mcr p15, 4, %0, c1,  c1, 0" : : "r"(hcr));
  asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->_regs.tpidruro));
}
