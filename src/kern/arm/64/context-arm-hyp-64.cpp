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
Context::arm_hyp_load_non_vm_state()
{
  if constexpr (Config::Have_mpu)
    asm volatile ("msr VTCR_EL2, %x0"    : : "r"(Cpu::vtcr_bits()));
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
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Vm_state *v)
{
  // save vm state

  // always trapped: asm volatile ("mrs %0, ACTLR_EL1" : "=r"(v->actlr));
  asm volatile ("mrs %x0, TCR_EL1"   : "=r"(v->tcr));
  asm volatile ("mrs %x0, TTBR0_EL1" : "=r"(v->ttbr0));
  if (EXPECT_TRUE(Cpu::boot_cpu_has_vmsa()))
    asm volatile ("mrs %x0, TTBR1_EL1" : "=r"(v->ttbr1));

  asm volatile ("mrs %x0, SCTLR_EL1" : "=r"(v->sctlr));
  asm volatile ("mrs %x0, ESR_EL1"   : "=r"(v->esr));

  asm volatile ("mrs %x0, MAIR_EL1"  : "=r"(v->mair));
  asm volatile ("mrs %x0, AMAIR_EL1" : "=r"(v->amair));

  asm volatile ("mrs %x0, FAR_EL1"   : "=r"(v->far));

  asm volatile ("mrs %x0, AFSR0_EL1" : "=r"(v->afsr[0]));
  asm volatile ("mrs %x0, AFSR1_EL1" : "=r"(v->afsr[1]));

  if (EXPECT_TRUE(Cpu::has_aarch32_el1()))
    {
      asm volatile ("mrs %x0, DACR32_EL2" : "=r"(v->dacr32));
      //asm volatile ("mrs %x0, FPEXC32_EL2" : "=r"(v->fpexc32));
      asm volatile ("mrs %x0, IFSR32_EL2" : "=r"(v->ifsr32));
    }

  if constexpr (Config::Have_mpu)
    {
      // Look into the HWs VTCR_EL2 because the user space could tamper
      // v->vtcr!
      Unsigned64 vtcr;
      asm ("mrs %x0, VTCR_EL2" : "=r"(vtcr));
      if (!(vtcr & Cpu::Vtcr_msa))
        save_ext_vcpu_state_mpu(v);
    }
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Vm_state const *v)
{
  Unsigned64 vtcr = 0;

  // always trapped: asm volatile ("msr ACTLR_EL1, %0" : : "r"(v->actlr));
  if constexpr (Config::Have_mpu)
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

  // Workaround for errata #852523 (Cortex-A57) and #853709 (Cortex-A72):
  // Do this before writing to SCTLR_EL1.
  if (EXPECT_TRUE(Cpu::has_aarch32_el1()))
    {
      asm volatile ("msr DACR32_EL2, %x0"  : : "r"(v->dacr32));
      //asm volatile ("msr FPEXC32_EL2, %x0" : : "r"(v->fpexc32));
      asm volatile ("msr IFSR32_EL2, %x0"  : : "r"(v->ifsr32));
    }

  asm volatile ("msr SCTLR_EL1, %x0" : : "r"(sctlr));
  asm volatile ("msr ESR_EL1, %x0"   : : "r"(v->esr));

  asm volatile ("msr MAIR_EL1, %x0"  : : "r"(v->mair));
  asm volatile ("msr AMAIR_EL1, %x0" : : "r"(v->amair));

  asm volatile ("msr FAR_EL1, %x0"   : : "r"(v->far));

  asm volatile ("msr AFSR0_EL1, %x0" : : "r"(v->afsr[0]));
  asm volatile ("msr AFSR1_EL1, %x0" : : "r"(v->afsr[1]));

  asm volatile ("msr VMPIDR_EL2, %x0" : : "r" (v->vmpidr));
  asm volatile ("msr VPIDR_EL2, %x0"  : : "r" (v->vpidr));

  if constexpr (Config::Have_mpu)
    {
      if (!(Cpu::boot_cpu_has_vmsa() && vtcr & Cpu::Vtcr_msa))
        load_ext_vcpu_state_mpu(v);
    }
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_host(Vcpu_state *vcpu, Vm_state *v)
{
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(vcpu->_regs.tpidruro));
  asm volatile ("mrs %x0, SCTLR_EL1"   : "=r"(v->guest_regs.sctlr));
  asm volatile ("mrs %x0, CPACR_EL1"   : "=r"(v->guest_regs.cpacr));
  asm volatile ("msr CPACR_EL1, %x0"   : : "r"(Cpu::Cpacr_el1_generic_hyp));

  asm volatile ("mrs %x0, CNTV_CTL_EL0" : "=r" (v->guest_regs.cntv_ctl));
  // disable VTIMER
  asm volatile ("msr CNTV_CTL_EL0, %x0" : : "r"(0UL));
  asm volatile ("msr CNTVOFF_EL2, %x0" : : "r"(0));
  // disable all debug exceptions for non-vms, if we want debug
  // exceptions into JDB we need either per-thread or a global
  // setting for this value. (probably including the contextidr)
  asm volatile ("msr MDSCR_EL1, %x0" : : "r"(0UL));
  asm volatile ("msr SCTLR_EL1, %x0" : : "r"(Cpu::Sctlr_el1_generic));

  _hyp.cntvoff = 0;
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
  _hyp.cntvoff  = 0;
  _hyp.cntv_ctl = 0;
  _hyp.cpacr    = Cpu::Cpacr_el1_generic_hyp;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_host_regs(Vcpu_state *vcpu, Vm_state *, Unsigned64 hcr)
{
  asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(vcpu->host.tpidruro));
  asm volatile ("msr HCR_EL2, %x0"     : : "r"(hcr));
  if constexpr (Config::Have_mpu)
    asm volatile ("msr VTCR_EL2, %x0"    : : "r"(Cpu::vtcr_bits()));
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
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *v, Unsigned64 hcr)
{
  asm volatile ("mrs %x0, TPIDRRO_EL0" : "=r"(vcpu->host.tpidruro));
  if constexpr (Config::Have_mpu)
    asm volatile ("msr VTCR_EL2, %x0"    : : "r"(v->vtcr & Cpu::Vtcr_usr_mask));
  asm volatile ("msr HCR_EL2, %x0"     : : "r"(hcr));
  asm volatile ("msr TPIDRRO_EL0, %x0" : : "r"(vcpu->_regs.tpidruro));
}

PRIVATE inline
void
Arm_vtimer_ppi::mask()
{
  Mword v;
  asm volatile("mrs %0, cntv_ctl_el0\n"
               "orr %0, %0, #0x2              \n"
               "msr cntv_ctl_el0, %0\n" : "=r" (v));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && !mpu]:

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

IMPLEMENT inline NEEDS["mpu.h"]
void
Context::save_ext_vcpu_state_mpu(Vm_state *v)
{
  v->mpu.prselr = Mpu_arm_el1::prselr();

#define SAVE_REGION(i) \
  do \
    { \
      Mword b = Mpu_arm_el1::prbar##i(); \
      Mword l = Mpu_arm_el1::prlar##i(); \
      v->mpu.r[31-idx].prbar = b; \
      v->mpu.r[31-idx].prlar = l; \
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
          case 15: SAVE_REGION(15); [[fallthrough]];
          case 14: SAVE_REGION(14); [[fallthrough]];
          case 13: SAVE_REGION(13); [[fallthrough]];
          case 12: SAVE_REGION(12); [[fallthrough]];
          case 11: SAVE_REGION(11); [[fallthrough]];
          case 10: SAVE_REGION(10); [[fallthrough]];
          case  9: SAVE_REGION(9);  [[fallthrough]];
          case  8: SAVE_REGION(8);  [[fallthrough]];
          case  7: SAVE_REGION(7);  [[fallthrough]];
          case  6: SAVE_REGION(6);  [[fallthrough]];
          case  5: SAVE_REGION(5);  [[fallthrough]];
          case  4: SAVE_REGION(4);  [[fallthrough]];
          case  3: SAVE_REGION(3);  [[fallthrough]];
          case  2: SAVE_REGION(2);  [[fallthrough]];
          case  1: SAVE_REGION(1);  [[fallthrough]];
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
      Mpu_arm_el1::prxar##i(v->mpu.r[31-idx].prbar, v->mpu.r[31-idx].prlar); \
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
          case 15: LOAD_REGION(15); [[fallthrough]];
          case 14: LOAD_REGION(14); [[fallthrough]];
          case 13: LOAD_REGION(13); [[fallthrough]];
          case 12: LOAD_REGION(12); [[fallthrough]];
          case 11: LOAD_REGION(11); [[fallthrough]];
          case 10: LOAD_REGION(10); [[fallthrough]];
          case  9: LOAD_REGION(9);  [[fallthrough]];
          case  8: LOAD_REGION(8);  [[fallthrough]];
          case  7: LOAD_REGION(7);  [[fallthrough]];
          case  6: LOAD_REGION(6);  [[fallthrough]];
          case  5: LOAD_REGION(5);  [[fallthrough]];
          case  4: LOAD_REGION(4);  [[fallthrough]];
          case  3: LOAD_REGION(3);  [[fallthrough]];
          case  2: LOAD_REGION(2);  [[fallthrough]];
          case  1: LOAD_REGION(1);  [[fallthrough]];
          case  0: LOAD_REGION(0);
            break;
        }
    }

#undef LOAD_REGION

  Mpu_arm_el1::prselr(v->mpu.prselr);
}
