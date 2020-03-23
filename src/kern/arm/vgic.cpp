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
  };

  using Arm_vgic = Arm_vgic_t<4>;

  virtual bool save_full(Arm_vgic *g) = 0;
  virtual void save_and_disable(Arm_vgic *g) = 0;
  virtual void load_full(Arm_vgic const *g, bool enabled) = 0;

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
    g->elsr = self()->elsr();
    self()->save_lrs(&g->lr, Arm_vgic::N_lregs);
    self()->save_aprs(g->aprs);
    return true;
  }

  void save_and_disable(Arm_vgic *g) override
  {
    if (Gic_h_mixin::save_full(g))
      self()->hcr(Hcr(0));
  }

  void load_full(Arm_vgic const *to, bool enabled) override
  {
    auto hcr = access_once(&to->hcr);
    if (hcr.en())
      {
        self()->vmcr(to->vmcr);
        self()->load_aprs(to->aprs);
        self()->load_lrs(&to->lr, Arm_vgic::N_lregs);
        self()->hcr(hcr);
      }
    else if (enabled)
      self()->hcr(hcr);
  }

  void setup_state(Arm_vgic *s) const override
  {
    s->hcr = Hcr(0);
    s->vtr = self()->vtr();
    for (auto &a: s->aprs)
      a = 0;
    for (auto &l: s->lr.lr64)
      l = 0;
  }

  void disable() override
  { self()->hcr(Hcr(0)); }

  unsigned version() const override
  { return IMPL::Version; }
};
