INTERFACE [arm && cpu_virt]:

#include "vgic.h"

EXTENSION class Context
{
public:
  struct Vm_state
  {
    struct Regs_g
    {
      Unsigned64 hcr;

      Unsigned32 sctlr;
      Unsigned32 cntkctl;
      Unsigned32 mdcr;
      Unsigned32 mdscr;
      Unsigned32 cpacr;
      // dummy to keep 64bit alignment and total size
      // of Regs_g + Regs_h pair
      Unsigned32 _dummy_;
    };

    struct Regs_h
    {
      Unsigned64 hcr;

      Unsigned32 dummy;
      Unsigned32 mdscr;
    };

    typedef Arm_vgic_t<4> Gic;

    /* The followin part is our user API */
    Regs_g guest_regs;
    Regs_h host_regs;
    Gic  gic;

    Unsigned64 vmpidr;

    Unsigned64 cntvoff;

    Unsigned64 cntv_cval;
    Unsigned32 cntkctl;
    Unsigned32 cntv_ctl;

    /* The user API ends here */

    /* we should align this at a cache line ... */
    Unsigned64 hcr;
    Unsigned64 actlr;

    Unsigned64 tcr;
    Unsigned64 ttbr0;
    Unsigned64 ttbr1;

    Unsigned32 csselr;
    Unsigned32 sctlr;
    Unsigned32 cpacr;
    Unsigned32 esr;

    Unsigned64 mair;
    Unsigned64 amair;

    Unsigned64 vbar;
    Unsigned64 tpidr_el1;
    Unsigned64 sp_el1;
    Unsigned64 elr_el1;

    Unsigned64 far;
    Unsigned64 par;

    Unsigned32 spsr_el1;
    Unsigned32 spsr_abt;
    Unsigned32 spsr_fiq;
    Unsigned32 spsr_irq;
    Unsigned32 spsr_und;

    Unsigned32 afsr[2];
    Unsigned32 mdcr;
    Unsigned32 mdscr;
    Unsigned32 contextidr;

    Unsigned32 dacr32;
    Unsigned32 fpexc32;
    Unsigned32 ifsr32;
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  unsigned const max_el = (state() & Thread_ext_vcpu_enabled) ? 1 : 0;

  if (dst->psr & 0x10)
    return;

  // set illegal execution state bit in PSR, this will trigger
  // an exception upon ERET
  if (((dst->psr & 0xf) >> 2) > max_el)
    dst->psr = dst->psr | (1UL << 20);
}

IMPLEMENT_OVERRIDE inline NEEDS["mem.h", Context::sanitize_user_state]
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{
  Mem::memcpy_mwords(&dst->r[0], &src->r[0], 31);
  dst->usp = src->usp;
  dst->pc = src->pc;
  dst->psr = access_once(&src->psr);
  sanitize_user_state(dst);
}

PRIVATE inline
void
Context::arm_hyp_load_non_vm_state(bool vgic)
{
  asm volatile ("msr hcr_el2, %0"   : : "r"(Cpu::Hcr_host_bits));
  // load normal SCTLR ...
  asm volatile ("msr sctlr_el1, %0" : : "r" (Cpu::Sctlr_el1_generic));
  asm volatile ("msr cpacr_el1, %0" : : "r" (0x300000));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value.
  asm volatile ("msr contextidr_el1, %0" : : "r"(0));
  asm volatile ("msr mdcr_el2, %0" : : "r"(Cpu::Mdcr_bits));
  asm volatile ("msr mdscr_el1, %0" : : "r"(0));
  asm volatile ("msr cntv_ctl_el0, %0" : : "r"(0)); // disable VTIMER
  // CNTKCTL: allow access to virtual and physical counter from PL0
  // see: generic_timer.cpp: setup_timer_access (Hyp)
  asm volatile("msr CNTKCTL_EL1, %0" : : "r"(0x3));
  if (vgic)
    Gic_h::gic->hcr(Gic_h::Hcr(0));
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Mword _state, Vm_state *v)
{
  // save vm state
  asm volatile ("mrs %0, TPIDRRO_EL0" : "=r"(_tpidruro));

  asm volatile ("mrs %0, HCR_EL2"   : "=r"(v->hcr));
  // always trapped: asm volatile ("mrs %0, ACTLR_EL1" : "=r"(v->actlr));
  asm volatile ("mrs %0, TCR_EL1"   : "=r"(v->tcr));
  asm volatile ("mrs %0, TTBR0_EL1" : "=r"(v->ttbr0));
  asm volatile ("mrs %0, TTBR1_EL1" : "=r"(v->ttbr1));

  asm volatile ("mrs %0, CSSELR_EL1" : "=r"(v->csselr));
  asm volatile ("mrs %0, SCTLR_EL1"  : "=r"(v->sctlr));
  asm volatile ("mrs %0, CPACR_EL1"  : "=r"(v->cpacr));
  asm volatile ("mrs %0, ESR_EL1"    : "=r"(v->esr));

  asm volatile ("mrs %0, MAIR_EL1"  : "=r"(v->mair));
  asm volatile ("mrs %0, AMAIR_EL1" : "=r"(v->amair));

  asm volatile ("mrs %0, VBAR_EL1"  : "=r"(v->vbar));
  asm volatile ("mrs %0, TPIDR_EL1" : "=r"(v->tpidr_el1));
  asm volatile ("mrs %0, SP_EL1"    : "=r"(v->sp_el1));
  asm volatile ("mrs %0, ELR_EL1"   : "=r"(v->elr_el1));

  asm volatile ("mrs %0, FAR_EL1"   : "=r"(v->far));
  asm volatile ("mrs %0, PAR_EL1"   : "=r"(v->par));

  asm volatile ("mrs %0, SPSR_EL1"  : "=r"(v->spsr_el1));
  asm volatile ("mrs %0, SPSR_abt"  : "=r"(v->spsr_abt));
  asm volatile ("mrs %0, SPSR_fiq"  : "=r"(v->spsr_fiq));
  asm volatile ("mrs %0, SPSR_irq"  : "=r"(v->spsr_irq));
  asm volatile ("mrs %0, SPSR_und"  : "=r"(v->spsr_und));

  asm volatile ("mrs %0, AFSR0_EL1" : "=r"(v->afsr[0]));
  asm volatile ("mrs %0, AFSR1_EL1" : "=r"(v->afsr[1]));

  asm volatile ("mrs %0, MDCR_EL2" : "=r"(v->mdcr));
  asm volatile ("mrs %0, MDSCR_EL1" : "=r"(v->mdscr));
  asm volatile ("mrs %0, CONTEXTIDR_EL1" : "=r"(v->contextidr));

  asm volatile ("mrs %0, DACR32_EL2"  : "=r"(v->dacr32));
//  asm volatile ("mrs %0, FPEXC32_EL2" : "=r"(v->fpexc32));
  asm volatile ("mrs %0, IFSR32_EL2"  : "=r"(v->ifsr32));

  asm volatile ("mrs %0, CNTV_CVAL_EL0" : "=r" (v->cntv_cval));
  asm volatile ("mrs %0, CNTVOFF_EL2"   : "=r" (v->cntvoff));
  asm volatile ("mrs %0, CNTKCTL_EL1"   : "=r" (v->cntkctl));
  if ((_state & Thread_vcpu_user))
    asm volatile ("mrs %0, CNTV_CTL_EL0" : "=r" (v->cntv_ctl));
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Mword _to_state, Vm_state const *v)
{
  asm volatile ("msr TPIDRRO_EL0, %0" : : "r"(_tpidruro));

  Mword hcr = access_once(&v->hcr) | Cpu::Hcr_must_set_bits;
  asm volatile ("msr HCR_EL2, %0"   : : "r"(hcr));
  // always trapped: asm volatile ("msr ACTLR_EL1, %0" : : "r"(v->actlr));

  asm volatile ("msr TCR_EL1, %0"   : : "r"(v->tcr));
  asm volatile ("msr TTBR0_EL1, %0" : : "r"(v->ttbr0));
  asm volatile ("msr TTBR1_EL1, %0" : : "r"(v->ttbr1));

  asm volatile ("msr CSSELR_EL1, %0" : : "r"(v->csselr));

  Unsigned32 sctlr = access_once(&v->sctlr);
  if (hcr & Cpu::Hcr_tge)
    sctlr &= ~Cpu::Cp15_c1_mmu;

  asm volatile ("msr SCTLR_EL1, %0"  : : "r"(sctlr));
  asm volatile ("msr CPACR_EL1, %0"  : : "r"(v->cpacr));
  asm volatile ("msr ESR_EL1, %0"    : : "r"(v->esr));

  asm volatile ("msr MAIR_EL1, %0"  : : "r"(v->mair));
  asm volatile ("msr AMAIR_EL1, %0" : : "r"(v->amair));

  asm volatile ("msr VBAR_EL1, %0"  : : "r"(v->vbar));
  asm volatile ("msr TPIDR_EL1, %0" : : "r"(v->tpidr_el1));
  asm volatile ("msr SP_EL1, %0"    : : "r"(v->sp_el1));
  asm volatile ("msr ELR_EL1, %0"   : : "r"(v->elr_el1));

  asm volatile ("msr FAR_EL1, %0"   : : "r"(v->far));
  asm volatile ("msr PAR_EL1, %0"   : : "r"(v->par));

  asm volatile ("msr SPSR_EL1, %0"  : : "r"(v->spsr_el1));
  asm volatile ("msr SPSR_abt, %0"  : : "r"(v->spsr_abt));
  asm volatile ("msr SPSR_fiq, %0"  : : "r"(v->spsr_fiq));
  asm volatile ("msr SPSR_irq, %0"  : : "r"(v->spsr_irq));
  asm volatile ("msr SPSR_und, %0"  : : "r"(v->spsr_und));

  asm volatile ("msr AFSR0_EL1, %0" : : "r"(v->afsr[0]));
  asm volatile ("msr AFSR1_EL1, %0" : : "r"(v->afsr[1]));

  Unsigned32 mdcr = access_once(&v->mdcr);
  mdcr &= Cpu::Mdcr_vm_mask;
  mdcr |= Cpu::Mdcr_bits;
  asm volatile ("msr MDCR_EL2, %0" : : "r"(mdcr));
  asm volatile ("msr MDSCR_EL1, %0" : : "r"(v->mdscr));
  asm volatile ("msr CONTEXTIDR_EL1, %0" : : "r"(v->contextidr));

  asm volatile ("msr DACR32_EL2, %0"  : : "r"(v->dacr32));
//  asm volatile ("msr FPEXC32_EL2, %0" : : "r"(v->fpexc32));
  asm volatile ("msr IFSR32_EL2, %0"  : : "r"(v->ifsr32));


  asm volatile ("msr VMPIDR_EL2, %0" : : "r" (v->vmpidr));

  asm volatile ("msr CNTV_CVAL_EL0, %0" : : "r" (v->cntv_cval));
  asm volatile ("msr CNTVOFF_EL2, %0"   : : "r" (v->cntvoff));
  asm volatile ("msr CNTKCTL_EL1, %0"   : : "r" (v->cntkctl));

  if ((_to_state & Thread_vcpu_user))
    asm volatile ("msr CNTV_CTL_EL0, %0" : : "r" (v->cntv_ctl));
  else
    asm volatile ("msr CNTV_CTL_EL0, %0" : : "r" (0));
}

PROTECTED static inline
Mword
Context::arm_get_hcr()
{
  Mword v;
  asm volatile ("mrs %0, HCR_EL2" : "=r"(v));
  return v;
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_host(Vcpu_state *vcpu, Vm_state *v)
{
  asm volatile ("mrs %0, TPIDRRO_EL0" : "=r"(vcpu->_regs.tpidruro));
  asm volatile ("mrs %0, SCTLR_EL1"   : "=r"(v->guest_regs.sctlr));
  asm volatile ("mrs %0, CNTKCTL_EL1" : "=r" (v->guest_regs.cntkctl));
  asm volatile ("mrs %0, MDCR_EL2"    : "=r"(v->guest_regs.mdcr));
  asm volatile ("mrs %0, MDSCR_EL1"   : "=r"(v->guest_regs.mdscr));
  asm volatile ("mrs %0, CPACR_EL1"   : "=r"(v->guest_regs.cpacr));
  asm volatile ("msr CPACR_EL1, %0"   : : "r"(3UL << 20));

  asm volatile ("msr CNTKCTL_EL1, %0"   : : "r" (Host_cntkctl));
  asm volatile ("mrs %0, CNTV_CTL_EL0" : "=r" (v->cntv_ctl));
  // disable VTIMER
  asm volatile ("msr CNTV_CTL_EL0, %0" : : "r"(0));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value. (probably including the contextidr)
  asm volatile ("msr MDCR_EL2, %0" : : "r"(Cpu::Mdcr_bits));
  asm volatile ("msr MDSCR_EL1, %0" : : "r"(0));
  asm volatile ("msr SCTLR_EL1, %0" : : "r"(Cpu::Sctlr_el1_generic));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_host_no_load(Vcpu_state *vcpu, Vm_state *v)
{
  vcpu->_regs.tpidruro     = _tpidruro;
  v->guest_regs.sctlr      = v->sctlr;
  v->guest_regs.cntkctl    = v->cntkctl;
  v->guest_regs.mdcr       = v->mdcr;
  v->guest_regs.mdscr      = v->mdscr;
  v->guest_regs.cpacr      = v->cpacr;

  v->cntkctl = Host_cntkctl;
  v->sctlr   = Cpu::Sctlr_el1_generic;
  v->cpacr   = 3UL << 20;
  v->mdcr    = Cpu::Mdcr_bits;
  v->mdscr   = 0;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_host_regs(Vcpu_state *vcpu, Vm_state *)
{
  asm volatile ("msr TPIDRRO_EL0, %0" : : "r"(vcpu->host.tpidruro));
  asm volatile ("msr HCR_EL2, %0"     : : "r"(Cpu::Hcr_host_bits));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest(Vcpu_state *, Vm_state *v)
{
  asm volatile ("msr VMPIDR_EL2, %0" : : "r" (v->vmpidr));
  asm volatile ("msr CNTKCTL_EL1, %0"   : : "r" (v->guest_regs.cntkctl));
  asm volatile ("msr CNTV_CTL_EL0, %0" : : "r" (v->cntv_ctl));
  Unsigned32 mdcr = access_once(&v->guest_regs.mdcr);
  mdcr &= Cpu::Mdcr_vm_mask;
  mdcr |= Cpu::Mdcr_bits;
  asm volatile ("msr SCTLR_EL1, %0"   : : "r"(v->guest_regs.sctlr));
  asm volatile ("msr MDCR_EL2, %0"    : : "r"(mdcr));
  asm volatile ("msr MDSCR_EL1, %0"   : : "r"(v->guest_regs.mdscr));
  asm volatile ("msr CPACR_EL1, %0"   : : "r"(v->guest_regs.cpacr));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest_no_load(Vcpu_state *, Vm_state *v)
{
  v->cntkctl = v->guest_regs.cntkctl;
  v->cpacr = v->guest_regs.cpacr;
  v->sctlr = v->guest_regs.sctlr;
  v->mdcr  = v->guest_regs.mdcr;
  v->mdscr = v->guest_regs.mdscr;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *, Mword hcr)
{
  asm volatile ("mrs %0, TPIDRRO_EL0" : "=r"(vcpu->host.tpidruro));
  asm volatile ("msr HCR_EL2, %0" : : "r"(hcr));
  asm volatile ("msr TPIDRRO_EL0, %0" : : "r"(vcpu->_regs.tpidruro));
}
