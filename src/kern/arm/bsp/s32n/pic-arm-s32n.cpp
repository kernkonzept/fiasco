/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

INTERFACE [arm && pic_gic && pf_s32n]:

#include "gic.h"
#include "gic_v3.h"
#include "initcalls.h"
#include "mmio_register_block.h"

class Mru : public Irq_chip_gen, Mmio_register_block
{
  // Helper to temporarily set VSCTLR.VMID to 0. Needed to access peripherals
  // from the kernel in case a VM is interrupted.
  class Vmid_guard
  {
    unsigned _vsctlr;

  public:
    Vmid_guard()
    {
      asm volatile ("mrc p15, 4, %0, c2, c0, 0" : "=r"(_vsctlr));
      asm volatile (
          "mcr p15, 4, %0, c2, c0, 0   \n" // VSCTLR
          "isb                         \n"
          : : "r"(0));
    }

    ~Vmid_guard()
    {
      asm volatile (
          "mcr p15, 4, %0, c2, c0, 0   \n" // VSCTLR
          : : "r"(_vsctlr));
    }
  };


private:
  enum {
    Mru_ppi_int0_id = 17,
    Mru_ppi_int1_id = 18,
    Mru_ppi_int2_id = 19,
    Mru_ppi_int3_id = 20,
  };

  enum
  {
    Notify0 = 0x200,
    Notify1 = 0x204,
    Notify2 = 0x208,
    Notify3 = 0x20c,

    Ch_offset = 0x10,
    Ch_cfg0   = 0x00,
    Ch_cfg1   = 0x04,
    Ch_mbstat = 0x08,
  };

  struct Cfg0
  {
    Unsigned32 raw;
    explicit Cfg0(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER( 0,  0, che, raw);
    CXX_BITFIELD_MEMBER( 1,  1, chr, raw);
    CXX_BITFIELD_MEMBER( 2,  2, ie, raw);
    CXX_BITFIELD_MEMBER(16, 23, mbe, raw);
  };

  struct Cfg1
  {
    Unsigned32 raw;
    explicit Cfg1(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(0, 15, mbic, raw);
  };

  struct Mru_ch_mode
  {
    Mword raw;
    explicit Mru_ch_mode(Mword v) : raw(v) {}

    CXX_BITFIELD_MEMBER(7, 7, set_mb_enable, raw);
    CXX_BITFIELD_MEMBER(8, 15, mbe, raw);
    CXX_BITFIELD_MEMBER(16, 31, mbic, raw);
  };

  class Mru_ppi : public Irq_base
  {
  public:
    Mru_ppi(Mru *mru)
    : _mru(mru)
    {
      set_hit(handler_wrapper<Mru_ppi>);
    }

    void init(Gic *parent, unsigned id)
    {
      check(parent->alloc(this, id, false));
      chip()->set_mode_percpu(current_cpu(), pin(),
                              Irq_chip::Mode::F_level_high);
      chip()->unmask_percpu(current_cpu(), pin());
    }

    void handle(Upstream_irq const *ui)
    {
      // Acknowledge before dispatching. The handle_multi_pending() method will
      // take the shortcut to user space.
      chip()->ack(pin());
      Upstream_irq::ack(ui);
      _mru->handle_multi_pending<Mru>(ui);
    }

  private:
    void switch_mode(bool) override {}

    Mru *_mru;
  };

  inline void cfg0(unsigned ch, Cfg0 v)
  { write<Unsigned32>(v.raw, ch * Ch_offset + Ch_cfg0); }

  inline Cfg0 cfg0(unsigned ch) const
  { return Cfg0(read<Unsigned32>(ch * Ch_offset + Ch_cfg0)); }

public:
  enum { Nr_irqs = 12 };

  explicit Mru(Address base, Gic *parent)
  : Irq_chip_gen(Nr_irqs), Mmio_register_block(base),
    _ppi0(this),
    _ppi1(this),
    _ppi2(this),
    _ppi3(this)
  {
    init(parent);
  }

private:
  Mru_ppi _ppi0;
  Mru_ppi _ppi1;
  Mru_ppi _ppi2;
  Mru_ppi _ppi3;
};

// ----------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_s32n]:

#include "irq_mgr_multi_chip.h"
#include "platform_control.h"
#include "kip.h"
#include "kmem.h"
#include "cpu.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<10> M;
  Mword id = Cpu::mpidr() & 0xffU;

  auto regs = Kmem::mmio_remap(Mem_layout::Gic_phys_base,
                               Mem_layout::Gic_phys_size);

  /* Distributor is initialized only by the first core of the cluster */
  *gic = new Boot_object<Gic_v3>(regs, regs + Mem_layout::Gic_redist_offset,
                                 id == 0);

  M *m = new Boot_object<M>(2);
  m->add_chip(0, *gic, (*gic)->nr_irqs());
  if (auto *mru = Mru::create_mru(*gic))
    m->add_chip(1024, mru, Mru::Nr_irqs);

  *Irq_mgr::mgr = m;
}


PRIVATE
void
Mru::init(Gic *parent)
{
  for (unsigned i = 0; i < Nr_irqs; i++)
    {
      // Reset channel. Disable if it was still enabled.
      Cfg0 c(0);
      c.chr() = 1;
      cfg0(i, c);

      // Enable channel with all mailboxes. If required the user space can
      // enable selective mailboxes with the set_mode() call.
      c.raw = 0;
      c.mbe() = 0xff;
      c.che() = 0xff;
      cfg0(i, c);   // will just set enable bit
      cfg0(i, c);   // now the mbe field is writable
    }

  _ppi0.init(parent, Mru_ppi_int0_id);
  _ppi1.init(parent, Mru_ppi_int1_id);
  _ppi2.init(parent, Mru_ppi_int2_id);
  _ppi3.init(parent, Mru_ppi_int3_id);
}

PUBLIC
Mword
Mru::pending()
{
  Vmid_guard g;
  return (read<Mword>(Notify0) | read<Mword>(Notify1)
          | read<Mword>(Notify2) | read<Mword>(Notify3));
}

PUBLIC
bool
Mru::alloc(Irq_base *irq, Mword pin, bool init = true) override
{
  bool ret = Irq_chip_gen::alloc(irq, pin, init);
  if (ret && init)
    irq->switch_mode(true); // it's edge triggered!
  return ret;
}

PUBLIC inline
void
Mru::mask(Mword pin) override final
{
  Vmid_guard g;

  Cfg0 c = cfg0(pin);
  c.ie() = 0;
  cfg0(pin, c);
}

PUBLIC inline
void
Mru::unmask(Mword pin) override final
{
  Vmid_guard g;

  Cfg0 c = cfg0(pin);
  c.ie() = 1;
  cfg0(pin, c);
}

PUBLIC inline
void
Mru::ack(Mword pin) override final
{
  Vmid_guard g;
  unsigned reg = pin * Ch_offset + Ch_mbstat;
  write<Unsigned32>(read<Unsigned32>(reg), reg);
}

PUBLIC
void
Mru::mask_and_ack(Mword pin) override
{
  ack(pin);
  mask(pin);
}

PUBLIC
int
Mru::set_mode(Mword pin, Mode m) override
{
  if (m.set_mode())
    {
      if (m.flow_type() != (Irq_chip::Mode::Trigger_edge
                            | Irq_chip::Mode::Polarity_high))
        return -L4_err::EInval;
    }

  Mru_ch_mode mm(m.raw);
  if (mm.set_mb_enable())
    {
      Vmid_guard g;
      unsigned base = pin * Ch_offset;

      Cfg1 c1 = Cfg1(read<Unsigned32>(base + Ch_cfg1));
      c1.mbic() = mm.mbic();
      write<Unsigned32>(c1.raw, base + Ch_cfg1);

      Cfg0 c0 = cfg0(pin);
      c0.mbe() = mm.mbe();
      cfg0(pin, c0);
    }

  return 0;
}

PUBLIC
bool
Mru::is_edge_triggered(Mword /*pin*/) const override
{
  return true;
}

PUBLIC
void
Mru::set_cpu(Mword /*pin*/, Cpu_number /*cpu*/) override
{
  // single core / AMP platform
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_s32n && pf_s32n_mru]:

PUBLIC static
Mru *
Mru::create_mru(Gic *gic)
{
  /* Note that the order is intentional because this is the way is mapped in hardware */
  static Address const mru_base[] = {
    Mem_layout::Rtu_Mru0, Mem_layout::Rtu_Mru2,
    Mem_layout::Rtu_Mru1, Mem_layout::Rtu_Mru3
  };

  Address mru_addr = mru_base[Platform_control::node_id()];
  Kip::k()->add_mem_region(Mem_desc(mru_addr,
                                    mru_addr + Mem_layout::Mru_size - 1,
	                            Mem_desc::Reserved, false, 0,
	                            1U << Platform_control::node_id()));
  auto mru_regs = Kmem::mmio_remap(mru_addr, Mem_layout::Mru_size);

  return new Boot_object<Mru>(mru_regs, gic);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_s32n && !pf_s32n_mru]:

PUBLIC static inline
Mru *
Mru::create_mru(Gic *)
{
  return nullptr;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_s32n && trace]:

PUBLIC
char const *
Mru::chip_type() const override
{
  return "MRU";
}
