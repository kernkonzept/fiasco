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
  asm volatile ("msr HCR_EL2, %0"   : : "r"(Cpu::Hcr_host_bits));
  // load normal SCTLR ...
  asm volatile ("msr SCTLR_EL1, %0" : : "r" ((Mword)Cpu::Sctlr_el1_generic));
  asm volatile ("msr CPACR_EL1, %0" : : "r" (0x300000UL));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value.
  asm volatile ("msr CONTEXTIDR_EL1, %0" : : "r"(0UL));
  asm volatile ("msr MDSCR_EL1, %0" : : "r"(0UL));
  asm volatile ("msr CNTV_CTL_EL0, %0" : : "r"(0UL)); // disable VTIMER
  // CNTKCTL: allow access to virtual and physical counter from PL0
  // see: generic_timer.cpp: setup_timer_access (Hyp)
  asm volatile("msr CNTKCTL_EL1, %0" : : "r"(0x3UL));
  if (vgic)
    Gic_h_global::gic->disable();
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Mword _state, Vm_state *v)
{
  Mword m;

  // save vm state
  asm volatile ("mrs %0, TPIDRRO_EL0" : "=r"(_tpidruro));

  asm volatile ("mrs %0, HCR_EL2"   : "=r"(v->hcr));
  // always trapped: asm volatile ("mrs %0, ACTLR_EL1" : "=r"(v->actlr));
  asm volatile ("mrs %0, TCR_EL1"   : "=r"(v->tcr));
  asm volatile ("mrs %0, TTBR0_EL1" : "=r"(v->ttbr0));
  asm volatile ("mrs %0, TTBR1_EL1" : "=r"(v->ttbr1));

  asm volatile ("mrs %0, CSSELR_EL1" : "=r"(m));
  v->csselr = m;
  asm volatile ("mrs %0, SCTLR_EL1"  : "=r"(m));
  v->sctlr = m;
  asm volatile ("mrs %0, CPACR_EL1"  : "=r"(m));
  v->cpacr = m;
  asm volatile ("mrs %0, ESR_EL1"    : "=r"(m));
  v->esr = m;

  asm volatile ("mrs %0, MAIR_EL1"  : "=r"(v->mair));
  asm volatile ("mrs %0, AMAIR_EL1" : "=r"(v->amair));

  asm volatile ("mrs %0, VBAR_EL1"  : "=r"(v->vbar));
  asm volatile ("mrs %0, TPIDR_EL1" : "=r"(v->tpidr_el1));
  asm volatile ("mrs %0, SP_EL1"    : "=r"(v->sp_el1));
  asm volatile ("mrs %0, ELR_EL1"   : "=r"(v->elr_el1));

  asm volatile ("mrs %0, FAR_EL1"   : "=r"(v->far));
  asm volatile ("mrs %0, PAR_EL1"   : "=r"(v->par));

  asm volatile ("mrs %0, SPSR_EL1"  : "=r"(m));
  v->spsr_el1 = m;
  asm volatile ("mrs %0, SPSR_abt"  : "=r"(m));
  v->spsr_abt = m;
  asm volatile ("mrs %0, SPSR_fiq"  : "=r"(m));
  v->spsr_fiq = m;
  asm volatile ("mrs %0, SPSR_irq"  : "=r"(m));
  v->spsr_irq = m;
  asm volatile ("mrs %0, SPSR_und"  : "=r"(m));
  v->spsr_und = m;

  asm volatile ("mrs %0, AFSR0_EL1" : "=r"(m));
  v->afsr[0] = m;
  asm volatile ("mrs %0, AFSR1_EL1" : "=r"(m));
  v->afsr[1] = m;

  asm volatile ("mrs %0, MDSCR_EL1" : "=r"(m));
  v->mdscr = m;
  asm volatile ("mrs %0, CONTEXTIDR_EL1" : "=r"(m));
  v->contextidr = m;

  asm volatile ("mrs %0, DACR32_EL2"  : "=r"(m));
  v->dacr32 = m;
//  asm volatile ("mrs %0, FPEXC32_EL2" : "=r"(m));
//  v->fpexc32 = m;
  asm volatile ("mrs %0, IFSR32_EL2"  : "=r"(m));
  v->ifsr32 = m;

  asm volatile ("mrs %0, CNTV_CVAL_EL0" : "=r" (v->cntv_cval));
  asm volatile ("mrs %0, CNTVOFF_EL2"   : "=r" (v->cntvoff));
  asm volatile ("mrs %0, CNTKCTL_EL1"   : "=r" (m));
  v->cntkctl = m;
  if ((_state & Thread_vcpu_user))
    {
      asm volatile ("mrs %0, CNTV_CTL_EL0" : "=r" (m));
      v->cntv_ctl = m;
    }
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

  asm volatile ("msr CSSELR_EL1, %0" : : "r"((Mword)v->csselr));

  Unsigned32 sctlr = access_once(&v->sctlr);
  if (hcr & Cpu::Hcr_tge)
    sctlr &= ~Cpu::Cp15_c1_mmu;

  asm volatile ("msr SCTLR_EL1, %0"  : : "r"((Mword)sctlr));
  asm volatile ("msr CPACR_EL1, %0"  : : "r"((Mword)v->cpacr));
  asm volatile ("msr ESR_EL1, %0"    : : "r"((Mword)v->esr));

  asm volatile ("msr MAIR_EL1, %0"  : : "r"(v->mair));
  asm volatile ("msr AMAIR_EL1, %0" : : "r"(v->amair));

  asm volatile ("msr VBAR_EL1, %0"  : : "r"(v->vbar));
  asm volatile ("msr TPIDR_EL1, %0" : : "r"(v->tpidr_el1));
  asm volatile ("msr SP_EL1, %0"    : : "r"(v->sp_el1));
  asm volatile ("msr ELR_EL1, %0"   : : "r"(v->elr_el1));

  asm volatile ("msr FAR_EL1, %0"   : : "r"(v->far));
  asm volatile ("msr PAR_EL1, %0"   : : "r"(v->par));

  asm volatile ("msr SPSR_EL1, %0"  : : "r"((Mword)v->spsr_el1));
  asm volatile ("msr SPSR_abt, %0"  : : "r"((Mword)v->spsr_abt));
  asm volatile ("msr SPSR_fiq, %0"  : : "r"((Mword)v->spsr_fiq));
  asm volatile ("msr SPSR_irq, %0"  : : "r"((Mword)v->spsr_irq));
  asm volatile ("msr SPSR_und, %0"  : : "r"((Mword)v->spsr_und));

  asm volatile ("msr AFSR0_EL1, %0" : : "r"((Mword)v->afsr[0]));
  asm volatile ("msr AFSR1_EL1, %0" : : "r"((Mword)v->afsr[1]));

  Unsigned32 mdcr = access_once(&v->mdcr);
  mdcr &= Cpu::Mdcr_vm_mask;
  mdcr |= Cpu::Mdcr_bits;
  asm volatile ("msr MDSCR_EL1, %0" : : "r"((Mword)v->mdscr));
  asm volatile ("msr CONTEXTIDR_EL1, %0" : : "r"((Mword)v->contextidr));

  asm volatile ("msr DACR32_EL2, %0"  : : "r"((Mword)v->dacr32));
//  asm volatile ("msr FPEXC32_EL2, %0" : : "r"((Mword)v->fpexc32));
  asm volatile ("msr IFSR32_EL2, %0"  : : "r"((Mword)v->ifsr32));


  asm volatile ("msr VMPIDR_EL2, %0" : : "r" (v->vmpidr));
  asm volatile ("msr VPIDR_EL2, %0"  : : "r" ((Mword)v->vpidr));

  asm volatile ("msr CNTV_CVAL_EL0, %0" : : "r" (v->cntv_cval));
  asm volatile ("msr CNTVOFF_EL2, %0"   : : "r" (v->cntvoff));
  asm volatile ("msr CNTKCTL_EL1, %0"   : : "r" ((Mword)(v->cntkctl)));

  if ((_to_state & Thread_vcpu_user))
    asm volatile ("msr CNTV_CTL_EL0, %0" : : "r" ((Mword)v->cntv_ctl));
  else
    asm volatile ("msr CNTV_CTL_EL0, %0" : : "r" (0UL));
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
  Mword m;

  asm volatile ("mrs %0, TPIDRRO_EL0" : "=r"(vcpu->_regs.tpidruro));
  asm volatile ("mrs %0, SCTLR_EL1"   : "=r"(m));
  v->guest_regs.sctlr = m;
  asm volatile ("mrs %0, CNTKCTL_EL1" : "=r"(m));
  v->guest_regs.cntkctl = m;
  asm volatile ("mrs %0, MDSCR_EL1"   : "=r"(m));
  v->guest_regs.mdscr = m;
  asm volatile ("mrs %0, CPACR_EL1"   : "=r"(m));
  v->guest_regs.cpacr = m;
  asm volatile ("msr CPACR_EL1, %0"   : : "r"(3UL << 20));

  asm volatile ("msr CNTKCTL_EL1, %0"  : : "r" ((Mword)Host_cntkctl));
  asm volatile ("mrs %0, CNTV_CTL_EL0" : "=r" (m));
  v->cntv_ctl = m;
  // disable VTIMER
  asm volatile ("msr CNTV_CTL_EL0, %0" : : "r"(0UL));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value. (probably including the contextidr)
  asm volatile ("msr MDSCR_EL1, %0" : : "r"(0UL));
  asm volatile ("msr SCTLR_EL1, %0" : : "r"((Mword)Cpu::Sctlr_el1_generic));
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
  asm volatile ("msr VPIDR_EL2, %0"  : : "r"((Mword)v->vpidr));
  asm volatile ("msr VMPIDR_EL2, %0" : : "r"(v->vmpidr));
  asm volatile ("msr CNTKCTL_EL1, %0" : : "r"((Mword)v->guest_regs.cntkctl));
  asm volatile ("msr CNTV_CTL_EL0, %0" : : "r"((Mword)v->cntv_ctl));
  Unsigned32 mdcr = access_once(&v->guest_regs.mdcr);
  mdcr &= Cpu::Mdcr_vm_mask;
  mdcr |= Cpu::Mdcr_bits;
  asm volatile ("msr SCTLR_EL1, %0"   : : "r"((Mword)v->guest_regs.sctlr));
  asm volatile ("msr MDSCR_EL1, %0"   : : "r"((Mword)v->guest_regs.mdscr));
  asm volatile ("msr CPACR_EL1, %0"   : : "r"((Mword)v->guest_regs.cpacr));
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
