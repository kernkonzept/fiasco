IMPLEMENTATION [arm && cpu_virt]:

#include "mem.h"

EXTENSION class Context
{
private:
  static inline void save_ext_vcpu_state_mxu(Vm_state *v);
  static inline void load_ext_vcpu_state_mxu(Vm_state const *v);
};

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
  if (_hyp.hcr & Cpu::Hcr_tge)
    {
      // Must run in user mode if HCR.TGE is set. Otherwise the behaviour
      // is unpredictable (Armv7) or leads to an illegal exception return
      // (Armv8).
      dst->psr = (dst->psr & ~Proc::Status_mode_mask) | Proc::PSR_m_usr;
    }
  else
    {
      // allow all but hyp or mon mode
      Unsigned32 const forbidden = ~0x888f0000U;
      if ((1UL << (dst->psr & Proc::Status_mode_mask)) & forbidden)
        dst->psr = (dst->psr & ~Proc::Status_mode_mask) | Proc::PSR_m_sys;
    }
}

IMPLEMENT_OVERRIDE inline NEEDS["mem.h", Context::sanitize_user_state]
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{
  // copy pf_addresss, esr, r0..r12, usp, ulr, km_lr
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

PRIVATE static inline NEEDS[Context::arm_host_sctlr]
void
Context::arm_hyp_load_non_vm_state()
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_non_vm)); // HSTR
  // load normal SCTLR ...
  asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r" (arm_host_sctlr())); // SCTLR
  asm volatile ("mcr p15, 0, %0,  c1, c0, 2" : : "r" (0xf00000));
  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r" (0));
}

PRIVATE static inline
void
Context::save_ext_vcpu_state(Vm_state *v)
{
  // save vm state
  asm volatile ("mrc p15, 2, %0, c0, c0, 0" : "=r"(v->csselr));

  asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r"(v->sctlr));
  // we unconditionally trap actlr accesses
  // asm ("mrc p15, 0, %0, c1, c0, 1" : "=r"(v->actlr));
  asm volatile ("mrc p15, 0, %0, c1, c0, 2" : "=r"(v->cpacr));

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

  save_ext_vcpu_state_mxu(v);
}

PRIVATE inline
void
Context::load_ext_vcpu_state(Vm_state const *v)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Cpu::Hstr_vm)); // HSTR
  asm volatile ("mcr p15, 2, %0, c0, c0, 0" : : "r"(v->csselr));

  Unsigned32 sctlr = access_once(&v->sctlr);
  if (_hyp.hcr & (Cpu::Hcr_tge | Cpu::Hcr_dc))
    sctlr &= ~Cpu::Sctlr_m;

  asm volatile ("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr));
  // we unconditionally trap actlr accesses
  // asm volatile ("mcr p15, 0, %0, c1, c0, 1" : : "r"(v->actlr));
  asm volatile ("mcr p15, 0, %0, c1, c0, 2" : : "r"(v->cpacr));

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

  load_ext_vcpu_state_mxu(v);
}

PROTECTED static constexpr
Unsigned32
Context::arm_host_sctlr()
{
  return (Cpu::sctlr | Cpu::Sctlr_cache_bits) & ~(Cpu::Sctlr_m | Cpu::Sctlr_tre);
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
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r"(0)); // CNTV_CTL
  asm volatile ("mcrr p15, 4, %Q0, %R0, c14" : : "r"(0ULL)); // CNTVOFF
  _hyp.cntvoff = 0;
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
  _hyp.cntvoff  = 0;
  _hyp.cntv_ctl = 0;
  v->fcseidr    = 0;
}

PRIVATE static inline
void
Context::arm_ext_vcpu_load_host_regs(Vcpu_state *vcpu, Vm_state *, Unsigned64 hcr)
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->host.tpidruro));
  Cpu::hcr(hcr);
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
}

PRIVATE static inline
void
Context::arm_ext_vcpu_load_guest_regs(Vcpu_state *vcpu, Vm_state *, Unsigned64 hcr)
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(vcpu->host.tpidruro));
  Cpu::hcr(hcr);
  asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->_regs.tpidruro));
}

PRIVATE inline
void
Arm_vtimer_ppi::mask()
{
  Mword v;
  asm volatile("mrc p15, 0, %0, c14, c3, 1\n"
               "orr %0, #0x2              \n"
               "mcr p15, 0, %0, c14, c3, 1\n" : "=r" (v));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && mmu]:

IMPLEMENT inline
void
Context::save_ext_vcpu_state_mxu(Vm_state *v)
{
  asm volatile ("mrrc p15, 0, %Q0, %R0, c2" : "=r"(v->mmu.ttbr0));
  asm volatile ("mrrc p15, 1, %Q0, %R0, c2" : "=r"(v->mmu.ttbr1));
  asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r"(v->mmu.ttbcr));
  asm volatile ("mrc p15, 0, %0, c3, c0, 0" : "=r"(v->mmu.dacr));
}

IMPLEMENT inline
void
Context::load_ext_vcpu_state_mxu(Vm_state const *v)
{
  asm volatile ("mcrr p15, 0, %Q0, %R0, c2" : : "r"(v->mmu.ttbr0));
  asm volatile ("mcrr p15, 1, %Q0, %R0, c2" : : "r"(v->mmu.ttbr1));
  asm volatile ("mcr p15, 0, %0, c2, c0, 2" : : "r"(v->mmu.ttbcr));
  asm volatile ("mcr p15, 0, %0, c3, c0, 0" : : "r"(v->mmu.dacr));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && mpu]:

#include "mpu.h"

IMPLEMENT inline NEEDS["mpu.h"]
void
Context::save_ext_vcpu_state_mxu(Vm_state * v)
{
  v->mpu.prselr = Mpu_arm_el1::prselr();

  // Don't use r7 as in THUMB2 mode, r7 is reserved as frame pointer register.
#define SAVE_REGIONS(i1, i2, i3, i4) \
  do \
    { \
      register Mword r0 asm("r0") = Mpu_arm_el1::prbar##i1(); \
      register Mword r1 asm("r1") = Mpu_arm_el1::prlar##i1(); \
      register Mword r2 asm("r2") = Mpu_arm_el1::prbar##i2(); \
      register Mword r3 asm("r3") = Mpu_arm_el1::prlar##i2(); \
      register Mword r4 asm("r4") = Mpu_arm_el1::prbar##i3(); \
      register Mword r5 asm("r5") = Mpu_arm_el1::prlar##i3(); \
      register Mword r6 asm("r6") = Mpu_arm_el1::prbar##i4(); \
      register Mword r8 asm("r8") = Mpu_arm_el1::prlar##i4(); \
      asm volatile ("stm %0!, {r0, r1, r2, r3, r4, r5, r6, r8}" \
        : "=&r"(ctx) \
        : "0"(ctx), \
          "r"(r0), \
          "r"(r1), \
          "r"(r2), \
          "r"(r3), \
          "r"(r4), \
          "r"(r5), \
          "r"(r6), \
          "r"(r8) \
        : "memory"); \
    } \
  while (false)

  // Directly skip non-existing regions. Assume multiple of 4 to make use of
  // stm instruction. They really make a difference on the write-through
  // caches of the R52.
  int idx = Mpu_arm_el1::regions();
  assert(idx <= 32);
  assert((idx & 0x3) == 0);

  Unsigned32 *ctx = &v->mpu.r[32 - idx].prbar;
  switch (idx >> 2)
    {
      case  8: SAVE_REGIONS(31, 30, 29, 28); [[fallthrough]];
      case  7: SAVE_REGIONS(27, 26, 25, 24); [[fallthrough]];
      case  6: SAVE_REGIONS(23, 22, 21, 20); [[fallthrough]];
      case  5: SAVE_REGIONS(19, 18, 17, 16); [[fallthrough]];
      case  4: SAVE_REGIONS(15, 14, 13, 12); [[fallthrough]];
      case  3: SAVE_REGIONS(11, 10,  9,  8); [[fallthrough]];
      case  2: SAVE_REGIONS( 7,  6,  5,  4); [[fallthrough]];
      case  1: SAVE_REGIONS( 3,  2,  1,  0); [[fallthrough]];
      default:
        break;
    }

#undef SAVE_REGIONS
}

IMPLEMENT inline NEEDS["mpu.h"]
void
Context::load_ext_vcpu_state_mxu(Vm_state const * v)
{
  // Don't use r7 as in THUMB2 mode, r7 is reserved as frame pointer register.
#define LOAD_REGIONS(i1, i2, i3, i4) \
  do \
    { \
      register Mword r0 asm("r0"); \
      register Mword r1 asm("r1"); \
      register Mword r2 asm("r2"); \
      register Mword r3 asm("r3"); \
      register Mword r4 asm("r4"); \
      register Mword r5 asm("r5"); \
      register Mword r6 asm("r6"); \
      register Mword r8 asm("r8"); \
      asm volatile ("ldm %0!, {r0, r1, r2, r3, r4, r5, r6, r8}" \
        : "=&r"(ctx), \
          "=r"(r0), \
          "=r"(r1), \
          "=r"(r2), \
          "=r"(r3), \
          "=r"(r4), \
          "=r"(r5), \
          "=r"(r6), \
          "=r"(r8) \
        : "0"(ctx) \
        : "memory"); \
      Mpu_arm_el1::prxar##i1(r0, r1); \
      Mpu_arm_el1::prxar##i2(r2, r3); \
      Mpu_arm_el1::prxar##i3(r4, r5); \
      Mpu_arm_el1::prxar##i4(r6, r8); \
    } \
  while (false)

  // Directly skip non-existing regions. Assume multiple of 4 to make use of
  // ldm instruction.
  int idx = Mpu_arm_el1::regions();
  assert(idx <= 32);
  assert((idx & 0x3) == 0);

  Unsigned32 const *ctx = &v->mpu.r[32 - idx].prbar;
  switch (idx >> 2)
    {
      case  8: LOAD_REGIONS(31, 30, 29, 28); [[fallthrough]];
      case  7: LOAD_REGIONS(27, 26, 25, 24); [[fallthrough]];
      case  6: LOAD_REGIONS(23, 22, 21, 20); [[fallthrough]];
      case  5: LOAD_REGIONS(19, 18, 17, 16); [[fallthrough]];
      case  4: LOAD_REGIONS(15, 14, 13, 12); [[fallthrough]];
      case  3: LOAD_REGIONS(11, 10,  9,  8); [[fallthrough]];
      case  2: LOAD_REGIONS( 7,  6,  5,  4); [[fallthrough]];
      case  1: LOAD_REGIONS( 3,  2,  1,  0); [[fallthrough]];
      default:
        break;
    }

#undef LOAD_REGIONS

  Mpu_arm_el1::prselr(v->mpu.prselr);
}
