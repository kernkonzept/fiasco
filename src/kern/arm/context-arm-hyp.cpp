INTERFACE [arm && hyp]:

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

      Per_mode_regs svc;
    };

    template< unsigned LREGS >
    struct Gic_t
    {
      enum { N_lregs = LREGS };
      Gic_h::Hcr hcr;
      Gic_h::Vtr vtr;
      Gic_h::Vmcr vmcr;
      Gic_h::Misr misr;
      Unsigned32 eisr[2];
      Unsigned32 elsr[2];
      Unsigned32 apr;
      Gic_h::Lr lr[LREGS];
    };

    typedef Gic_t<4> Gic;

    /* The followin part is our user API */
    Regs guest_regs;
    Regs host_regs;
    Gic  gic;

    Unsigned64 cntvoff;

    Unsigned64 cntv_cval;
    Unsigned32 cntkctl;
    Unsigned32 cntv_ctl;

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
IMPLEMENTATION [arm && hyp]:

#include "mem.h"

IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  if (state() & Thread_ext_vcpu_enabled)
    {
      if ((dst->psr & Proc::Status_mode_mask) == Proc::PSR_m_hyp)
        dst->psr = (dst->psr & ~Proc::Status_mode_mask) | Proc::PSR_m_usr;
    }
  else
    {
      dst->psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
      dst->psr |= Proc::Status_mode_user | Proc::Status_always_mask;
    }
}

IMPLEMENT_OVERRIDE inline NEEDS["mem.h", Context::sanitize_user_state]
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{
  Mem::memcpy_mwords(dst, src, 19);
  dst->pc = src->pc;
  dst->psr = access_once(&src->psr);
  sanitize_user_state(dst);
}

IMPLEMENT inline
void
Context::fill_user_state()
{
}

IMPLEMENT inline
void
Context::spill_user_state()
{
}

PROTECTED static inline
Context::Vm_state *
Context::vm_state(Vcpu_state *vs)
{ return reinterpret_cast<Vm_state *>(reinterpret_cast<char *>(vs) + 0x400); }

PRIVATE inline
void
Context::arm_hyp_load_non_vm_state(bool vgic)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 0"
                : : "r"(Cpu::Hcr_tge | Cpu::Hcr_dc | Cpu::Hcr_must_set_bits));
  // load normal SCTLR ...
  asm volatile ("mcr p15, 0, %0, c1, c0, 0"
                : : "r" ((Cpu::Cp15_c1_generic | Cpu::Cp15_c1_cache_bits) & ~Cpu::Cp15_c1_mmu));
  asm volatile ("mcr p15, 0, %0,  c1, c0, 2" : : "r" (0xf00000));
  asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r" (0));
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r" (0));
  asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (0)); // disable VTIMER
  if (vgic)
    Gic_h::gic->hcr(Gic_h::Hcr(0));
}

PUBLIC inline NEEDS[Context::vm_state, Context::arm_hyp_load_non_vm_state]
void
Context::switch_vm_state(Context *t)
{
  Mword _state = state();
  Mword _to_state = t->state();
  if (!((_state | _to_state) & Thread_ext_vcpu_enabled))
    return;

  bool vgic = false;

  if (_state & Thread_ext_vcpu_enabled)
    {
      // save vm state
      Vm_state *v = vm_state(vcpu_state().access());
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

      if ((_state & Thread_vcpu_user) && Gic_h::gic->hcr().en())
        {
          vgic = true;
          v->gic.vmcr = Gic_h::gic->vmcr();
          v->gic.misr = Gic_h::gic->misr();

          for (unsigned i = 0; i < ((Vm_state::Gic::N_lregs + 31) / 32); ++i)
            v->gic.eisr[i] = Gic_h::gic->eisr(i);

          for (unsigned i = 0; i < ((Vm_state::Gic::N_lregs + 31) / 32); ++i)
            v->gic.elsr[i] = Gic_h::gic->elsr(i);

          v->gic.apr = Gic_h::gic->apr();

          for (unsigned i = 0; i < Vm_state::Gic::N_lregs; ++i)
            v->gic.lr[i] = Gic_h::gic->lr(i);
        }
    }

  if (_to_state & Thread_ext_vcpu_enabled)
    {
      Vm_state const *v = vm_state(t->vcpu_state().access());
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

      if ((_to_state & Thread_vcpu_user) && v->gic.hcr.en())
        {
          Gic_h::gic->vmcr(v->gic.vmcr);
          Gic_h::gic->apr(v->gic.apr);
          for (unsigned i = 0; i < Vm_state::Gic::N_lregs; ++i)
            Gic_h::gic->lr(i, v->gic.lr[i]);
          Gic_h::gic->hcr(v->gic.hcr);
        }
      else if (vgic)
        Gic_h::gic->hcr(Gic_h::Hcr(0));
    }
  else
    arm_hyp_load_non_vm_state(vgic);
}

IMPLEMENT_OVERRIDE
void
Context::arch_vcpu_ext_shutdown()
{
  if (!(state() & Thread_ext_vcpu_enabled))
    return;

  state_del_dirty(Thread_ext_vcpu_enabled);
  arm_hyp_load_non_vm_state(Gic_h::gic->hcr().en());
}

IMPLEMENT_OVERRIDE inline NEEDS[Context::vm_state]
void
Context::arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load)
{
  if (!(state() & Thread_ext_vcpu_enabled))
    {
      _tpidruro = vcpu->host.tpidruro;
      if (do_load)
        load_tpidruro();
      return;
    }

  Vm_state *v = vm_state(vcpu);
  Unsigned32 hcr;
  if (do_load)
    asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(hcr));
  else
    hcr = access_once(&v->hcr);

  v->guest_regs.hcr = hcr;
  bool const all_priv_vm = !(hcr & Cpu::Hcr_tge);
  if (all_priv_vm)
    {
      // save guest state, load full host state
      if (do_load)
        {
          asm volatile ("mrc p15, 0, %0, c13, c0, 3"
                        : "=r"(vcpu->_regs.s.tpidruro));
          asm volatile ("mrc p15, 0, %0, c1,  c0, 0"
                        : "=r"(v->guest_regs.sctlr));
          asm volatile ("mrc p15, 0, %0, c13, c0, 0"
                        : "=r"(v->guest_regs.fcseidr));
          asm volatile ("mrc p15, 0, %0, c13, c0, 1"
                        : "=r"(v->guest_regs.contextidr));

          asm volatile ("mrs %0, SP_svc"   : "=r"(v->guest_regs.svc.sp));
          asm volatile ("mrs %0, LR_svc"   : "=r"(v->guest_regs.svc.lr));
          asm volatile ("mrs %0, SPSR_svc" : "=r"(v->guest_regs.svc.spsr));

          // fcse not supported in vmm
          asm volatile ("mcr p15, 0, %0, c13, c0, 0" : : "r"(0));
          asm volatile ("mcr p15, 0, %0, c13, c0, 1"
                        : : "r"(v->host_regs.contextidr));

          asm volatile ("msr SP_svc, %0"   : : "r"(v->host_regs.svc.sp));
          asm volatile ("msr LR_svc, %0"   : : "r"(v->host_regs.svc.lr));
          asm volatile ("msr SPSR_svc, %0" : : "r"(v->host_regs.svc.spsr));

          asm volatile ("mrc p15, 0, %0, c14, c3, 1" : "=r" (v->cntv_ctl));
          // disable VTIMER
          asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r"(0));

          Gic_h::Hcr ghcr = Gic_h::gic->hcr();
          if (ghcr.en())
            {
              v->gic.hcr = ghcr;
              v->gic.vmcr = Gic_h::gic->vmcr();
              v->gic.misr = Gic_h::gic->misr();
              for (unsigned i = 0; i < ((Vm_state::Gic::N_lregs + 31) / 32); ++i)
                v->gic.eisr[i] = Gic_h::gic->eisr(i);
              for (unsigned i = 0; i < ((Vm_state::Gic::N_lregs + 31) / 32); ++i)
                v->gic.elsr[i] = Gic_h::gic->elsr(i);
              v->gic.apr = Gic_h::gic->apr();
              for (unsigned i = 0; i < Vm_state::Gic::N_lregs; ++i)
                v->gic.lr[i] = Gic_h::gic->lr(i);
              Gic_h::gic->hcr(Gic_h::Hcr(0));
            }
        }
      else
        {
          vcpu->_regs.s.tpidruro = _tpidruro;
          v->guest_regs.sctlr      = v->sctlr;
          v->guest_regs.fcseidr    = v->fcseidr;
          v->guest_regs.contextidr = v->contextidr;
          v->guest_regs.svc        = v->svc;

          v->fcseidr    = 0;
          v->contextidr = v->host_regs.contextidr;
          v->svc        = v->host_regs.svc;
        }
    }

  _tpidruro = vcpu->host.tpidruro;
  if (do_load)
    {
      asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->host.tpidruro));
      asm volatile ("mcr p15, 4, %0, c1,  c1, 0" : : "r"(Cpu::Hcr_must_set_bits | Cpu::Hcr_dc));
      asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r"(v->host_regs.sctlr));
    }
  else
    {
      v->hcr   = Cpu::Hcr_must_set_bits | Cpu::Hcr_dc;
      v->sctlr = v->host_regs.sctlr;
    }
}

IMPLEMENT_OVERRIDE inline NEEDS[Context::vm_state]
void
Context::arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load)
{

  if (!(state() & Thread_ext_vcpu_enabled))
    {
      _tpidruro = vcpu->_regs.s.tpidruro;
      if (do_load)
        load_tpidruro();
      return;
    }

  Vm_state *v = vm_state(vcpu);
  Unsigned32 hcr = access_once(&v->guest_regs.hcr) | Cpu::Hcr_must_set_bits;
  bool const all_priv_vm = !(hcr & Cpu::Hcr_tge);

  if (all_priv_vm)
    {
      if (do_load)
        {
          asm volatile ("mrc p15, 0, %0, c13, c0, 1"
                        : "=r"(v->host_regs.contextidr));

          asm volatile ("mrs %0, SP_svc"   : "=r"(v->host_regs.svc.sp));
          asm volatile ("mrs %0, LR_svc"   : "=r"(v->host_regs.svc.lr));
          asm volatile ("mrs %0, SPSR_svc" : "=r"(v->host_regs.svc.spsr));

          asm volatile ("mcr p15, 0, %0, c13, c0, 0"
                        : : "r"(v->guest_regs.fcseidr));
          asm volatile ("mcr p15, 0, %0, c13, c0, 1"
                        : : "r"(v->guest_regs.contextidr));

          asm volatile ("msr SP_svc, %0"   : : "r"(v->guest_regs.svc.sp));
          asm volatile ("msr LR_svc, %0"   : : "r"(v->guest_regs.svc.lr));
          asm volatile ("msr SPSR_svc, %0" : : "r"(v->guest_regs.svc.spsr));

          asm volatile ("mcr p15, 0, %0, c14, c3, 1" : : "r" (v->cntv_ctl));

          if (v->gic.hcr.en())
            {
              Gic_h::gic->vmcr(v->gic.vmcr);
              Gic_h::gic->apr(v->gic.apr);
              for (unsigned i = 0; i < Vm_state::Gic::N_lregs; ++i)
                Gic_h::gic->lr(i, v->gic.lr[i]);
            }
          Gic_h::gic->hcr(v->gic.hcr);
        }
      else
        {
          v->host_regs.svc = v->svc;

          v->fcseidr    = v->guest_regs.fcseidr;
          v->contextidr = v->guest_regs.contextidr;
          v->svc        = v->guest_regs.svc;
        }
    }

  if (do_load)
    {
      asm volatile ("mrc p15, 0, %0, c13, c0, 3" : "=r"(vcpu->host.tpidruro));
      asm volatile ("mrc p15, 0, %0, c1,  c0, 0" : "=r"(v->host_regs.sctlr));

      asm volatile ("mcr p15, 4, %0, c1,  c1, 0" : : "r"(hcr));
      Unsigned32 sctlr = access_once(&v->guest_regs.sctlr);
      if (hcr & Cpu::Hcr_tge)
        sctlr &= ~Cpu::Cp15_c1_mmu;
      asm volatile ("mcr p15, 0, %0, c1,  c0, 0" : : "r"(sctlr));
      asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r"(vcpu->_regs.s.tpidruro));
      _tpidruro          = vcpu->_regs.s.tpidruro;
    }
  else
    {
      vcpu->host.tpidruro = _tpidruro;
      _tpidruro           = vcpu->_regs.s.tpidruro;
      v->host_regs.sctlr  = v->sctlr;
      v->hcr              = hcr;
      v->sctlr            = v->guest_regs.sctlr;
    }
}
