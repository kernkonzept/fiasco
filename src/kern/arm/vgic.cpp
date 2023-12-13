INTERFACE [arm && cpu_virt && vgic]:

#include "panic.h"
#include "types.h"

#include <cxx/bitfield>

class Gic_h
{
public:
  struct Hcr
  {
    Unsigned32 raw;
    Hcr() = default;
    explicit Hcr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  0, en, raw);
    CXX_BITFIELD_MEMBER(  1,  1, uie, raw);
    CXX_BITFIELD_MEMBER(  2,  2, lr_en_pie, raw);
    CXX_BITFIELD_MEMBER(  3,  3, n_pie, raw);
    CXX_BITFIELD_MEMBER(  4,  4, vgrp0_eie, raw);
    CXX_BITFIELD_MEMBER(  5,  5, vgrp0_die, raw);
    CXX_BITFIELD_MEMBER(  6,  6, vgrp1_eie, raw);
    CXX_BITFIELD_MEMBER(  7,  7, vgrp1_die, raw);
    //  <<< GICv3+ only
    CXX_BITFIELD_MEMBER( 10, 10, tc, raw);
    CXX_BITFIELD_MEMBER( 11, 11, tall0, raw);
    CXX_BITFIELD_MEMBER( 12, 12, tall1, raw);
    CXX_BITFIELD_MEMBER( 13, 13, tsei, raw);
    CXX_BITFIELD_MEMBER( 14, 14, tdir, raw);
    CXX_BITFIELD_MEMBER( 15, 15, dvim, raw);
    // >>> GICv3+ only
    CXX_BITFIELD_MEMBER( 27, 31, eoi_cnt, raw);
  };

  struct Vtr
  {
    Unsigned32 raw;
    Vtr() = default;
    explicit Vtr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  5, list_regs, raw);
    CXX_BITFIELD_MEMBER( 23, 25, id_bits, raw); // v3
    CXX_BITFIELD_MEMBER( 26, 28, pre_bits, raw);
    CXX_BITFIELD_MEMBER( 29, 31, pri_bits, raw);
  };

  struct Vmcr
  {
    Unsigned32 raw;
    Vmcr() = default;
    explicit Vmcr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  0, grp0_en, raw);
    CXX_BITFIELD_MEMBER(  1,  1, grp1_en, raw);
    CXX_BITFIELD_MEMBER(  2,  2, ack_ctl, raw);
    CXX_BITFIELD_MEMBER(  3,  3, fiq_en, raw);
    CXX_BITFIELD_MEMBER(  4,  4, cbpr, raw);
    CXX_BITFIELD_MEMBER(  9,  9, vem, raw);
    CXX_BITFIELD_MEMBER( 18, 20, abp, raw);
    CXX_BITFIELD_MEMBER( 21, 23, bp, raw);
    CXX_BITFIELD_MEMBER( 24, 31, vmpr, raw); // v3
    CXX_BITFIELD_MEMBER( 27, 31, pri_mask, raw);
  };

  struct Misr
  {
    Unsigned32 raw;
    Misr() = default;
    explicit Misr(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0,  0, eoi, raw);
    CXX_BITFIELD_MEMBER(  1,  1, u, raw);
    CXX_BITFIELD_MEMBER(  2,  2, lrenp, raw);
    CXX_BITFIELD_MEMBER(  3,  3, np, raw);
    CXX_BITFIELD_MEMBER(  4,  4, grp0_e, raw);
    CXX_BITFIELD_MEMBER(  5,  5, grp0_d, raw);
    CXX_BITFIELD_MEMBER(  6,  6, grp1_e, raw);
    CXX_BITFIELD_MEMBER(  7,  7, grp1_d, raw);
  };

  struct Vcpu_irq_cfg
  {
    Mword raw;
    Vcpu_irq_cfg() = default;
    explicit Vcpu_irq_cfg(Mword v) : raw(v) {}

    CXX_BITFIELD_MEMBER(  0, 19,  vid, raw);
    CXX_BITFIELD_MEMBER( 23, 23, grp1, raw);
    CXX_BITFIELD_MEMBER( 24, 31, prio, raw);
  };

  template< unsigned LREGS >
  struct Arm_vgic_t
  {
    enum { N_lregs = LREGS };
    union Lrs
    {
      Unsigned64 lr64[LREGS];
      Unsigned32 lr32[LREGS];
    };

    Gic_h::Hcr hcr;
    Gic_h::Vtr vtr;
    Gic_h::Vmcr vmcr;
    Gic_h::Misr misr;
    Unsigned32 eisr;
    Unsigned32 elsr;
    Lrs lr;
    Unsigned32 aprs[8];
    Unsigned32 injected;
  };

  using Arm_vgic = Arm_vgic_t<4>;

  enum class From_vgic_mode { Disabled, Enabled };
  enum class To_user_mode { Disabled, Enabled };

  /**
   * Switch away from thread in extended vCPU mode and save the vGIC context if
   * that context has the vGIC enabled (HCR.En() == 1).
   *
   * \retval From_vgic_mode::Disabled vGIC interface is disabled for this thread.
   * \retval From_vgic_mode::Enabled vGIC interface is enabled for this thread.
   */
  virtual From_vgic_mode switch_from_vcpu(Arm_vgic *g) = 0;

  /**
   * Switch between two threads *not* in extended vCPU mode.
   *
   * \param from_mode  From_vgic_mode::Enabled when switching from a thread in
   *                   extended vCPU user mode which has the vGIC interface
   *                   enabled. From_vgic_mode::Disabled when switching from a
   *                   thread either not in extended vCPU user mode or having
   *                   the vGIC interface disabled.
   */
  virtual void switch_to_non_vcpu(From_vgic_mode from_mode) = 0;

  /**
   * Switch to a thread in extended vCPU mode.
   *
   * \param g          vGIC context of the thread.
   * \param to_mode    To_user_mode::Enabled when switching to a thread in
   *                   extended vCPU user mode. To_user_mode::Disabled when
   *                   switching to a thread not in extended vCPU user mode.
   * \param from_mode  From_vgic_mode::Enabled when switching from a thread in
   *                   extended vCPU user mode which has the vGIC interface
   *                   enabled. From_vgic_mode::Disabled when switching from a thread
   *                   either not in extended vCPU user mode or having the vGIC
   *                   interface disabled.
   */
  virtual void switch_to_vcpu(Arm_vgic const *g, To_user_mode to_mode,
                              From_vgic_mode from_mode) = 0;

  /**
   * Called while switching from extended vCPU user mode and entering extended
   * vCPU kernel mode -- save the vGIC state and disable the vGIC interface.
   */
  virtual void save_and_disable(Arm_vgic *g) = 0;

  /**
   * Disable the vGIC interface -- either during kernel initialization or if the
   * current context has to access to the vGIC interface.
   */
  virtual void disable() = 0;

  /**
   * Inject a vIRQ into the extended vCPU user mode.
   *
   * \param g     vGIC context of the thread.
   * \param cfg   vIRQ specification
   * \param load  Shall the hardware be updated immediately?
   *
   * \return  Used list register index plus one or zero in case no LR was free.
   */
  virtual unsigned inject(Arm_vgic *g, Vcpu_irq_cfg cfg, bool load) = 0;

  /**
   * Revoke pending vIRQ from extended vCPU user mode.
   *
   * \param g       vGIC context of the thread.
   * \param lr      List register index that shall be cleared.
   * \param load    Shall the hardware be updated immediately?
   * \param abandon Abandon vIRQ if it was active.
   *
   * \return  True if vIRQ was active, false if it was still pending.
   */
  virtual bool revoke(Arm_vgic *g, unsigned lr, bool load, bool abandon) = 0;

  /**
   * Handle vGIC maintenance interrupt.
   *
   * Handle the state for vIRQs that were previously injected by inject().
   * Will free list registers where the guest EOIed the interrupt.
   *
   * \param g         vGIC context of the thread.
   * \param eois[out] Bitmap of list registers that were EOIed
   *
   * \return  True if maintenance interrupt was completely handled, false
   *          if a switch to vCPU kernel mode is (still) necessary.
   */
  virtual bool handle_maintenance(Arm_vgic *g, Unsigned32 *eois) = 0;

  /**
   * Return the vGIC version.
   */
  virtual unsigned version() const = 0;

  /**
   * Return the virtual address of the MMIO interface.
   * Returns the map address for GICv2 and 0 for GICv3.
   */
  virtual Address gic_v_address() const = 0;

  virtual void setup_state(Arm_vgic *s) const = 0;
};

template<typename IMPL>
class Gic_h_mixin : public Gic_h
{
private:
  IMPL const *self() const { return static_cast<IMPL const *>(this); }
  IMPL *self() { return static_cast<IMPL *>(this); }

protected:
  /**
   * Switch to a thread in extended CPU mode.
   *
   * \retval false vGIC disabled for this thread, vGIC state unchanged.
   * \retval true vGIC enabled for this thread, vGIC state restored.
   */
  bool switch_to_vcpu_user(Arm_vgic const *g)
  {
    auto hcr = access_once(&g->hcr);
    if (!hcr.en())
      return false;

    self()->vmcr(g->vmcr);
    self()->load_aprs(g->aprs);
    self()->load_lrs(&g->lr);
    self()->hcr(hcr);
    IMPL::vgic_barrier();
    return true;
  }

public:
  From_vgic_mode switch_from_vcpu(Arm_vgic *g) override final
  {
    auto hcr = self()->hcr();
    if (!hcr.en())
      return From_vgic_mode::Disabled;

    // the EIOcount might have changed
    g->hcr  = hcr;
    g->vmcr = self()->vmcr();
    g->misr = self()->misr();
    g->eisr = self()->eisr();
    // Only report saved/loaded LR registers as free
    g->elsr = self()->elsr() & ((1U << Arm_vgic::N_lregs) - 1U);
    // Hide any pending maintenance of injected interrupts from user space!
    if (EXPECT_FALSE(g->injected))
      {
        g->eisr &= ~g->injected;
        if (g->eisr == 0)
          g->misr.eoi() = 0;
      }
    self()->save_lrs(&g->lr);
    self()->save_aprs(g->aprs);
    return From_vgic_mode::Enabled;
  }

  void disable() override final
  {
    self()->disable_load_defaults();
    IMPL::vgic_barrier();
  }

  unsigned inject(Arm_vgic *g, Vcpu_irq_cfg cfg, bool load) override
  {
    unsigned idx = __builtin_ffs((load ? self()->elsr() : g->elsr) &
                                 ((1UL << Arm_vgic::N_lregs) - 1U));
    if (!idx)
      return 0;

    self()->build_lr(&g->lr, idx - 1, cfg, load);
    g->injected |= 1UL << (idx - 1);
    if (!load)
      g->elsr &= ~(1UL << (idx - 1));

    return idx;
  }

  bool revoke(Arm_vgic *g, unsigned idx, bool load, bool abandon) override
  {
    bool active = self()->teardown_lr(&g->lr, idx);
    if (!active)
      {
        // Was pending but not active. Completely remove it.
        if (load)
          self()->clear_lr(idx);
        g->injected &= ~(1UL << idx);
        g->elsr |= 1UL << idx;
      }
    else if (abandon)
      {
        // Has been active and we should abandon it. From here on, it is the
        // responsibility of the VMM to handle the eventual EOI of the guest
        // and to finally clear the LR.
        g->injected &= ~(1UL << idx);
      }

    return active;
  }

  bool handle_maintenance(Arm_vgic *g, Unsigned32 *eois) override
  {
    if (!g->injected)
      {
        *eois = 0;
        return false;
      }

    Unsigned32 eisr = self()->eisr() & g->injected;
    *eois = eisr;
    g->injected &= ~eisr;

    while (eisr)
      {
        unsigned idx = __builtin_ffs(eisr) - 1U;
        eisr &= ~(1UL << idx);
        self()->clear_lr(idx);
      }

    return self()->misr().raw == 0;
  }

  void setup_state(Arm_vgic *s) const override
  {
    s->hcr = Hcr(0);
    s->vtr = self()->vtr();

    // We assume that the GIC implements at least Arm_vgic::N_lregs line
    // registers. Ensure that is really the case, because accessing a
    // non-implemented list register would result in an Undefined
    // Instruction Exception.
    if (s->vtr.list_regs() + 1 < Arm_vgic::N_lregs)
      panic("GIC implements fewer virtual line registers than required.");

    // Clamp number of supported LRs to the actually saved/loaded LRs. The
    // others are not usable to user space.
    if (s->vtr.list_regs() >= Arm_vgic::N_lregs)
      s->vtr.list_regs() = Arm_vgic::N_lregs - 1;

    for (auto &a: s->aprs)
      a = 0;
    for (auto &l: s->lr.lr64)
      l = 0;
    s->injected = 0;
  }

  unsigned version() const override
  { return IMPL::Version; }
};
