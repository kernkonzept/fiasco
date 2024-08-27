IMPLEMENTATION [arm && cpu_virt]:

EXTENSION class Context
{
private:
  inline void save_ext_vcpu_state_mpu(Vm_state *v);
  inline void load_ext_vcpu_state_mpu(Vm_state const *v);
};

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
  // omit eret_work, ksp, esr, pf_address
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
  if (Cpu::Vtcr_usr_mask != 0)
    asm volatile ("msr VTCR_EL2, %x0"    : : "r"(Cpu::Vtcr_bits));
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
  asm volatile("msr CNTHCTL_EL2, %x0"   : : "r"(Host_cnthctl));
  if (vgic)
    (*Gic_h_global::gic)->disable();
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Mword /*_state*/, Vm_state *v)
{
  // save vm state

  // always trapped: asm volatile ("mrs %0, ACTLR_EL1" : "=r"(v->actlr));
  asm volatile ("mrs %x0, TCR_EL1"   : "=r"(v->tcr));
  asm volatile ("mrs %x0, TTBR0_EL1" : "=r"(v->ttbr0));
  asm volatile ("mrs %x0, TTBR1_EL1" : "=r"(v->ttbr1)); // FIXME: not always present on ARMv8-R!

  asm volatile ("mrs %x0, SCTLR_EL1" : "=r"(v->sctlr));
  asm volatile ("mrs %x0, ESR_EL1"   : "=r"(v->esr));

  asm volatile ("mrs %x0, MAIR_EL1"  : "=r"(v->mair));
  asm volatile ("mrs %x0, AMAIR_EL1" : "=r"(v->amair));

  asm volatile ("mrs %x0, FAR_EL1"   : "=r"(v->far));

  asm volatile ("mrs %x0, AFSR0_EL1" : "=r"(v->afsr[0]));
  asm volatile ("mrs %x0, AFSR1_EL1" : "=r"(v->afsr[1]));

//  asm volatile ("mrs %x0, DACR32_EL2" : "=r"(v->dacr32));
//  asm volatile ("mrs %x0, FPEXC32_EL2" : "=r"(v->fpexc32));
//  asm volatile ("mrs %x0, IFSR32_EL2" : "=r"(v->ifsr32));

  if (Cpu::Vtcr_usr_mask != 0)
    {
      // Look into the HWs VTCR_EL2 because the user space could tamper
      // v->vtcr!
      Unsigned64 vtcr;
      asm ("mrs %x0, VTCR_EL2" : "=r"(vtcr));
      if (!(vtcr & (1UL << 31)))
        save_ext_vcpu_state_mpu(v);
    }
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Mword _to_state, Vm_state const *v)
{
  Unsigned64 vtcr = 0;

  // always trapped: asm volatile ("msr ACTLR_EL1, %0" : : "r"(v->actlr));
  if (Cpu::Vtcr_usr_mask != 0)
    {
      vtcr = access_once(&v->vtcr);
      asm volatile ("msr VTCR_EL2, %x0"    : : "r"(vtcr & Cpu::Vtcr_usr_mask));
    }
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

//  asm volatile ("msr DACR32_EL2, %x0"  : : "r"(v->dacr32));
//  asm volatile ("msr FPEXC32_EL2, %x0" : : "r"(v->fpexc32));
//  asm volatile ("msr IFSR32_EL2, %x0"  : : "r"(v->ifsr32));


  asm volatile ("msr VMPIDR_EL2, %x0" : : "r" (v->vmpidr));
  asm volatile ("msr VPIDR_EL2, %x0"  : : "r" (v->vpidr));

  if (_to_state & Thread_vcpu_user)
    asm volatile("msr CNTHCTL_EL2, %x0"   : : "r"(Guest_cnthctl));
  else
    asm volatile("msr CNTHCTL_EL2, %x0"   : : "r"(Host_cnthctl));

  if (Cpu::Vtcr_usr_mask != 0 && !(vtcr & (1UL << 31)))
    load_ext_vcpu_state_mpu(v);
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
  asm volatile ("msr CNTHCTL_EL2, %x0"  : : "r"(Host_cnthctl));
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
  if (Cpu::Vtcr_usr_mask != 0)
    asm volatile ("msr VTCR_EL2, %x0"    : : "r"(Cpu::Vtcr_bits));
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
  asm volatile ("msr CNTHCTL_EL2, %x0" : : "r"(Guest_cnthctl));
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

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && !mpu]:

PRIVATE inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *, Mword hcr)
{
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(vcpu->host.tpidruro));
  asm volatile ("msr HCR_EL2, %x0"     : : "r"(hcr));
  asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(vcpu->_regs.tpidruro));
}

IMPLEMENT inline
void
Context::save_ext_vcpu_state_mpu(Vm_state *)
{}

IMPLEMENT inline
void
Context::load_ext_vcpu_state_mpu(Vm_state const *)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && mpu]:

#include "mpu.h"

PRIVATE inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *v, Mword hcr)
{
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(vcpu->host.tpidruro));
  if (Cpu::Vtcr_usr_mask != 0)
    asm volatile ("msr VTCR_EL2, %x0"    : : "r"(v->vtcr & Cpu::Vtcr_usr_mask));
  asm volatile ("msr HCR_EL2, %x0"     : : "r"(hcr));
  asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(vcpu->_regs.tpidruro));
}

IMPLEMENT inline NEEDS["mpu.h"]
void
Context::save_ext_vcpu_state_mpu(Vm_state *v)
{
  v->mpu.prselr = Mpu_arm_el1::prselr();

#define SAVE_REGION(i) \
  do \
    { \
      v->mpu.r[31-idx].prbar = Mpu_arm_el1::prbar##i(); \
      v->mpu.r[31-idx].prlar = Mpu_arm_el1::prlar##i(); \
      --idx; \
    } \
  while (false)

  // Directly skip non-existing regions.
  int idx = Mpu_arm_el1::regions() - 1;
  assert(idx < 32);
  while (idx >= 0)
    {
      Mpu_arm_el1::prselr(idx & 0xf0);
      switch (idx & 0x0f)
        {
          case 15: SAVE_REGION(15); // fall through
          case 14: SAVE_REGION(14); // fall through
          case 13: SAVE_REGION(13); // fall through
          case 12: SAVE_REGION(12); // fall through
          case 11: SAVE_REGION(11); // fall through
          case 10: SAVE_REGION(10); // fall through
          case  9: SAVE_REGION(9);  // fall through
          case  8: SAVE_REGION(8);  // fall through
          case  7: SAVE_REGION(7);  // fall through
          case  6: SAVE_REGION(6);  // fall through
          case  5: SAVE_REGION(5);  // fall through
          case  4: SAVE_REGION(4);  // fall through
          case  3: SAVE_REGION(3);  // fall through
          case  2: SAVE_REGION(2);  // fall through
          case  1: SAVE_REGION(1);  // fall through
          case  0: SAVE_REGION(0);
            break;
        }
    }

#undef SAVE_REGION
}

IMPLEMENT inline NEEDS["mpu.h"]
void
Context::load_ext_vcpu_state_mpu(Vm_state const *v)
{
#define LOAD_REGION(i) \
  do \
    { \
      Mpu_arm_el1::prbar##i(v->mpu.r[31-idx].prbar); \
      Mpu_arm_el1::prlar##i(v->mpu.r[31-idx].prlar); \
      --idx; \
    } \
  while (false)

  // Directly skip non-existing regions.
  int idx = Mpu_arm_el1::regions() - 1;
  assert(idx < 32);
  while (idx >= 0)
    {
      Mpu_arm_el1::prselr(idx & 0xf0);
      switch (idx & 0x0f)
        {
          case 15: LOAD_REGION(15); // fall through
          case 14: LOAD_REGION(14); // fall through
          case 13: LOAD_REGION(13); // fall through
          case 12: LOAD_REGION(12); // fall through
          case 11: LOAD_REGION(11); // fall through
          case 10: LOAD_REGION(10); // fall through
          case  9: LOAD_REGION(9);  // fall through
          case  8: LOAD_REGION(8);  // fall through
          case  7: LOAD_REGION(7);  // fall through
          case  6: LOAD_REGION(6);  // fall through
          case  5: LOAD_REGION(5);  // fall through
          case  4: LOAD_REGION(4);  // fall through
          case  3: LOAD_REGION(3);  // fall through
          case  2: LOAD_REGION(2);  // fall through
          case  1: LOAD_REGION(1);  // fall through
          case  0: LOAD_REGION(0);
            break;
        }
    }

#undef LOAD_REGION

  Mpu_arm_el1::prselr(v->mpu.prselr);
}
