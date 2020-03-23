IMPLEMENTATION [arm && cpu_virt]:

IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  unsigned const max_el = 1;

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
Context::store_tpidruro()
{
  // store TPIDRURO here, loading is done in normal thread switch
  // code already
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(_tpidruro));
}

PRIVATE inline
void
Context::arm_hyp_load_non_vm_state(bool vgic)
{
  asm volatile ("msr HCR_EL2, %x0"   : : "r"(Cpu::Hcr_non_vm_bits));
  asm volatile ("msr HSTR_EL2, %x0"  : : "r"(Cpu::Hstr_non_vm));
  // load normal SCTLR ...
  asm volatile ("msr SCTLR_EL1, %x0" : : "r"(Cpu::Sctlr_el1_generic));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value.
  asm volatile ("msr MDSCR_EL1, %x0"    : : "r"(0UL));
  asm volatile ("msr CNTV_CTL_EL0, %x0" : : "r"(0UL)); // disable VTIMER
  // CNTKCTL: allow access to virtual and physical counter from PL0
  // see: generic_timer.cpp: setup_timer_access (Hyp)
  asm volatile("msr CNTKCTL_EL1, %x0"   : : "r"(0x3UL));
  if (vgic)
    Gic_h_global::gic->disable();
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Mword /*_state*/, Vm_state *v)
{
  // save vm state

  // always trapped: asm volatile ("mrs %0, ACTLR_EL1" : "=r"(v->actlr));
  asm volatile ("mrs %x0, TCR_EL1"   : "=r"(v->tcr));
  asm volatile ("mrs %x0, TTBR0_EL1" : "=r"(v->ttbr0));
  asm volatile ("mrs %x0, TTBR1_EL1" : "=r"(v->ttbr1));

  asm volatile ("mrs %x0, SCTLR_EL1" : "=r"(v->sctlr));
  asm volatile ("mrs %x0, ESR_EL1"   : "=r"(v->esr));

  asm volatile ("mrs %x0, MAIR_EL1"  : "=r"(v->mair));
  asm volatile ("mrs %x0, AMAIR_EL1" : "=r"(v->amair));

  asm volatile ("mrs %x0, FAR_EL1"   : "=r"(v->far));

  asm volatile ("mrs %x0, AFSR0_EL1" : "=r"(v->afsr[0]));
  asm volatile ("mrs %x0, AFSR1_EL1" : "=r"(v->afsr[1]));

  asm volatile ("mrs %x0, DACR32_EL2" : "=r"(v->dacr32));
//  asm volatile ("mrs %x0, FPEXC32_EL2" : "=r"(v->fpexc32));
  asm volatile ("mrs %x0, IFSR32_EL2" : "=r"(v->ifsr32));
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Mword /*_to_state*/, Vm_state const *v)
{
  // always trapped: asm volatile ("msr ACTLR_EL1, %0" : : "r"(v->actlr));
  asm volatile ("msr HSTR_EL2, %x0" : : "r"(Cpu::Hstr_vm)); // HSTR

  asm volatile ("msr TCR_EL1, %x0"   : : "r"(v->tcr));
  asm volatile ("msr TTBR0_EL1, %x0" : : "r"(v->ttbr0));
  asm volatile ("msr TTBR1_EL1, %x0" : : "r"(v->ttbr1));

  Unsigned32 sctlr = access_once(&v->sctlr);
  if (_hyp.hcr & (Cpu::Hcr_tge | Cpu::Hcr_dc))
    sctlr &= ~Cpu::Cp15_c1_mmu;

  asm volatile ("msr SCTLR_EL1, %x0" : : "r"(sctlr));
  asm volatile ("msr ESR_EL1, %x0"   : : "r"(v->esr));

  asm volatile ("msr MAIR_EL1, %x0"  : : "r"(v->mair));
  asm volatile ("msr AMAIR_EL1, %x0" : : "r"(v->amair));

  asm volatile ("msr FAR_EL1, %x0"   : : "r"(v->far));

  asm volatile ("msr AFSR0_EL1, %x0" : : "r"(v->afsr[0]));
  asm volatile ("msr AFSR1_EL1, %x0" : : "r"(v->afsr[1]));

  asm volatile ("msr DACR32_EL2, %x0"  : : "r"(v->dacr32));
//  asm volatile ("msr FPEXC32_EL2, %x0" : : "r"(v->fpexc32));
  asm volatile ("msr IFSR32_EL2, %x0"  : : "r"(v->ifsr32));


  asm volatile ("msr VMPIDR_EL2, %x0" : : "r" (v->vmpidr));
  asm volatile ("msr VPIDR_EL2, %x0"  : : "r" (v->vpidr));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_host(Vcpu_state *vcpu, Vm_state *v)
{
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(vcpu->_regs.tpidruro));
  asm volatile ("mrs %x0, SCTLR_EL1"   : "=r"(v->guest_regs.sctlr));
  asm volatile ("mrs %x0, CPACR_EL1"   : "=r"(v->guest_regs.cpacr));
  asm volatile ("msr CPACR_EL1, %x0"   : : "r"(3UL << 20));

  asm volatile ("mrs %x0, CNTV_CTL_EL0" : "=r" (v->guest_regs.cntv_ctl));
  // disable VTIMER
  asm volatile ("msr CNTV_CTL_EL0, %x0" : : "r"(0UL));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value. (probably including the contextidr)
  asm volatile ("msr MDSCR_EL1, %x0" : : "r"(0UL));
  asm volatile ("msr SCTLR_EL1, %x0" : : "r"(Cpu::Sctlr_el1_generic));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_host_no_load(Vcpu_state *vcpu, Vm_state *v)
{
  vcpu->_regs.tpidruro   = _tpidruro;
  v->guest_regs.sctlr    = v->sctlr;
  v->guest_regs.cpacr    = _hyp.cpacr;
  v->guest_regs.cntv_ctl = _hyp.cntv_ctl;

  v->sctlr   = Cpu::Sctlr_el1_generic;
  _hyp.cntv_ctl = 0;
  _hyp.cpacr    = 3UL << 20;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_host_regs(Vcpu_state *vcpu, Vm_state *)
{
  asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(vcpu->host.tpidruro));
  asm volatile ("msr HCR_EL2, %x0"     : : "r"(Cpu::Hcr_host_bits));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest(Vcpu_state *, Vm_state *v)
{
  asm volatile ("msr VPIDR_EL2, %x0"  : : "r"(v->vpidr));
  asm volatile ("msr VMPIDR_EL2, %x0" : : "r"(v->vmpidr));
  _hyp.cntvoff = v->cntvoff;
  asm volatile ("msr CNTVOFF_EL2, %x0" : : "r"(v->cntvoff));
  asm volatile ("msr CNTV_CTL_EL0, %x0": : "r"(v->guest_regs.cntv_ctl));
  asm volatile ("msr SCTLR_EL1, %x0"   : : "r"(v->guest_regs.sctlr));
  asm volatile ("msr CPACR_EL1, %x0"   : : "r"(v->guest_regs.cpacr));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest_no_load(Vcpu_state *, Vm_state *v)
{
  v->sctlr = v->guest_regs.sctlr;
  _hyp.cpacr = v->guest_regs.cpacr;
  _hyp.cntv_ctl = v->guest_regs.cntv_ctl;
  _hyp.cntvoff  = v->cntvoff;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *, Mword hcr)
{
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(vcpu->host.tpidruro));
  asm volatile ("msr HCR_EL2, %x0"     : : "r"(hcr));
  asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(vcpu->_regs.tpidruro));
}
