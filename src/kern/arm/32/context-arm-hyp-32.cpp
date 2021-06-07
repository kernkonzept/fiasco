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
  Unsigned32 const forbidden = ~0x888f0000U;  // allow all but hyp mode
  if ((1UL << (dst->psr & Proc::Status_mode_mask)) & forbidden)
    dst->psr = (dst->psr & ~Proc::Status_mode_mask) | Proc::PSR_m_sys;
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
Context::store_tpidruro()
{
  // store TPIDRURO here, loading is done in normal thread switch
  // code already
  asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(_tpidruro));
}

PRIVATE inline
void
Context::arm_hyp_load_non_vm_state(bool vgic)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 0"
                : : "r"(Cpu::Hcr_non_vm_bits));
  asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_non_vm)); // HSTR
  // load normal SCTLR ...
  asm volatile ("mcr p15, 0, %0, c1, c0, 0"
                : : "r" ((Cpu::sctlr | Cpu::Cp15_c1_cache_bits) & ~Cpu::Cp15_c1_mmu));
  asm volatile ("mcr p15, 0, %0,  c1, c0, 2" : : "r" (0xf00000));
  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r" (0));
  asm volatile("mcr p15, 4, %0, c14, c1, 0" : : "r"(Host_cnthctl));

  if (vgic)
    Gic_h_global::gic->disable();
}

PRIVATE inline
void
Context::save_ext_vcpu_state(Mword /*_state*/, Vm_state *v)
{
  // save vm state
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

  asm volatile ("mrc p15, 0, %0, c10, c2, 0" : "=r"(v->mair0));
  asm volatile ("mrc p15, 0, %0, c10, c2, 1" : "=r"(v->mair1));

  asm volatile ("mrc p15, 0, %0, c10, c3, 0" : "=r"(v->amair0));
  asm volatile ("mrc p15, 0, %0, c10, c3, 1" : "=r"(v->amair1));

  asm volatile ("mrc p15, 0, %0, c12, c0, 0" : "=r"(v->vbar));

  asm volatile ("mrc p15, 0, %0, c13, c0, 0" : "=r"(v->fcseidr));
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Mword /*_to_state*/, Vm_state const *v)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_vm)); // HSTR
  asm volatile ("mcr p15, 2, %0, c0, c0, 0" : : "r"(v->csselr));

  Unsigned32 sctlr = access_once(&v->sctlr);
  if (_hyp.hcr & (Cpu::Hcr_tge | Cpu::Hcr_dc))
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

  asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(v->mair0));
  asm volatile ("mcr p15, 0, %0, c10, c2, 1" : : "r"(v->mair1));

  asm volatile ("mcr p15, 0, %0, c10, c3, 0" : : "r"(v->amair0));
  asm volatile ("mcr p15, 0, %0, c10, c3, 1" : : "r"(v->amair1));

  asm volatile ("mcr p15, 0, %0, c12, c0, 0" : : "r"(v->vbar));

  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(v->fcseidr));

  asm volatile ("mcr  p15, 4, %0, c0, c0, 5" : : "r" (v->vmpidr));
  asm volatile ("mcr  p15, 4, %0, c0, c0, 0" : : "r" (v->vpidr));
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

  // fcse not supported in vmm
  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(0));
  asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r"(arm_host_sctlr()));

  asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (v->guest_regs.cntv_ctl));
  // disable VTIMER
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r"(0));
  asm volatile ("mcr p15, 4, %0, c14, c1, 0" : : "r"(Host_cnthctl));
}

PRIVATE inline NEEDS[Context::arm_host_sctlr]
void
Context::arm_ext_vcpu_switch_to_host_no_load(Vcpu_state *vcpu, Vm_state *v)
{
  vcpu->_regs.tpidruro     = _tpidruro;
  v->guest_regs.sctlr      = v->sctlr;
  v->guest_regs.fcseidr    = v->fcseidr;
  v->guest_regs.cntv_ctl   = _hyp.cntv_ctl;

  v->sctlr      = arm_host_sctlr();
  _hyp.cntv_ctl = 0;
  v->fcseidr    = 0;
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
  asm volatile ("mcr p15, 0, %0, c13, c0, 0"
                : : "r"(v->guest_regs.fcseidr));

  asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r" (v->guest_regs.sctlr));
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (v->guest_regs.cntv_ctl));
  _hyp.cntvoff = v->cntvoff;
  asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r" (v->cntvoff));

  asm volatile ("mcr  p15, 4, %0, c0, c0, 5" : : "r" (v->vmpidr));
  asm volatile ("mcr  p15, 4, %0, c0, c0, 0" : : "r" (v->vpidr));
  asm volatile ("mcr  p15, 4, %0, c14, c1, 0" : : "r" (Guest_cnthctl));
}

PRIVATE inline
void
Context::arm_ext_vcpu_switch_to_guest_no_load(Vcpu_state *, Vm_state *v)
{
  v->fcseidr    = v->guest_regs.fcseidr;
  v->sctlr      = v->guest_regs.sctlr;
  _hyp.cntv_ctl = v->guest_regs.cntv_ctl;
  _hyp.cntvoff  = v->cntvoff;
}

PRIVATE inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *, Unsigned64 hcr)
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(vcpu->host.tpidruro));
  Cpu::hcr(hcr);
  asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->_regs.tpidruro));
}
