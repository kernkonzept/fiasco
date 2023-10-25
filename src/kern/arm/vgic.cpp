INTERFACE [arm && cpu_virt && vgic]:

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

  struct Irq_prio_cfg
  {
    Unsigned32 raw;
    Irq_prio_cfg() = default;
    explicit Irq_prio_cfg(Unsigned32 v) : raw(v) {}

    CXX_BITFIELD_MEMBER(  0,  7, base, raw);
    CXX_BITFIELD_MEMBER(  8, 15, mult, raw);
    CXX_BITFIELD_MEMBER( 16, 16, map, raw);   ///< Enable priority map

    Unsigned8 map_irq_prio(Unsigned8 irq_prio) const
    {
      return map() ? ((255U - irq_prio) * mult() / 255U + base()) : 0xff;
    }
  };

  // Attention: keep compatible to Hyp_vm_state::Vtmr
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
    Irq_prio_cfg irq_prio_cfg;
    Unsigned32 injected;
  };

  using Arm_vgic = Arm_vgic_t<4>;

  virtual bool save_full(Arm_vgic *g) = 0;
  virtual void save_and_disable(Arm_vgic *g) = 0;
  virtual Unsigned8 load_full(Arm_vgic const *g, bool enabled) = 0;
  virtual unsigned inject(Arm_vgic *g, Vcpu_irq_cfg cfg, bool load,
                          Unsigned8 *irq_prio) = 0;
  virtual bool revoke(Arm_vgic *g, unsigned lr, bool load, bool reap) = 0;
  virtual bool handle_maintenance(Arm_vgic *g, Unsigned32 *eois) = 0;
  virtual Unsigned8 calc_irq_priority(Arm_vgic *g) = 0;

  virtual void disable() = 0;
  virtual unsigned version() const = 0;
  virtual Address gic_v_address() const = 0;
  virtual void setup_state(Arm_vgic *s) const = 0;
};

template<typename IMPL>
class Gic_h_mixin : public Gic_h
{
private:
  IMPL const *self() const { return static_cast<IMPL const *>(this); }
  IMPL *self() { return static_cast<IMPL *>(this); }

public:
  bool save_full(Arm_vgic *g) override
  {
    auto hcr = self()->hcr();
    if (!hcr.en())
      return false;

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
    self()->save_lrs(&g->lr, Arm_vgic::N_lregs);
    self()->save_aprs(g->aprs);
    return true;
  }

  void save_and_disable(Arm_vgic *g) override
  {
    if (Gic_h_mixin::save_full(g))
      disable();
  }

  Unsigned8 load_full(Arm_vgic const *to, bool enabled) override
  {
    Unsigned8 ret = 0xff;
    auto hcr = access_once(&to->hcr);
    if (hcr.en())
      {
        Irq_prio_cfg cfg = access_once(&to->irq_prio_cfg);
        self()->vmcr(to->vmcr);
        self()->load_aprs(to->aprs);
        ret = cfg.map_irq_prio(self()->load_lrs(&to->lr, Arm_vgic::N_lregs));
        self()->hcr(hcr);
        IMPL::vgic_barrier();
      }
    else if (enabled)
      {
        self()->hcr(hcr);
        IMPL::vgic_barrier();
      }

    return ret;
  }

  unsigned inject(Arm_vgic *g, Vcpu_irq_cfg cfg, bool load,
                  Unsigned8 *irq_prio) override
  {
    unsigned idx = __builtin_ffs((load ? self()->elsr() : g->elsr) &
                                 ((1UL << Arm_vgic::N_lregs) - 1U));
    if (!idx)
      return 0;

    self()->build_lr(&g->lr, idx-1, cfg, load);
    g->injected |= 1UL << (idx-1);
    if (load)
      {
        Irq_prio_cfg prio_cfg = access_once(&g->irq_prio_cfg);
        if (irq_prio)
          *irq_prio = prio_cfg.map_irq_prio(cfg.prio());
      }
    else
      g->elsr &= ~(1UL << (idx-1));

    return idx;
  }

  bool revoke(Arm_vgic *g, unsigned idx, bool load, bool reap) override
  {
    bool ret = self()->teardown_lr(&g->lr, idx, reap);
    if (!ret)
      {
        if (load)
          self()->clear_lr(idx);
        g->injected &= ~(1UL << idx);
        g->elsr |= 1UL << idx;
      }

    return ret;
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

  Unsigned8 calc_irq_priority(Arm_vgic *g) override
  {
    Irq_prio_cfg cfg = access_once(&g->irq_prio_cfg);
    if (!cfg.map())
      return 0xff;

    return cfg.map_irq_prio(self()->scan_lrs(&g->lr, Arm_vgic::N_lregs));
  }

  void setup_state(Arm_vgic *s) const override
  {
    s->hcr = Hcr(0);
    s->vtr = self()->vtr();
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

  void disable() override final
  {
    self()->hcr(Hcr(0));
    IMPL::vgic_barrier(); // Ensure vgic is disabled before going to user-level
  }

  unsigned version() const override
  { return IMPL::Version; }
};
