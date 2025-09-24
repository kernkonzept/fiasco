INTERFACE:

#include "irq_chip_generic.h"
#include "mmio_register_block.h"
#include "context_base.h"

class Mru : public Irq_chip_gen, Mmio_register_block
{
  enum
  {
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

  // Extend Irq_chip::Mode. Use additional mode bits for MRU channel set_mode()
  // calls.
  struct Mru_ch_mode : Mode
  {
    using Mode::Mode;
    Mru_ch_mode(Mode const &m) : Mode(m.raw) {}

    CXX_BITFIELD_MEMBER(15, 15, set_mb_enable, raw);
    CXX_BITFIELD_MEMBER(16, 23, mbe, raw);
  };

  class Mru_ppi : public Irq_base
  {
  public:
    Mru_ppi(Mru *mru)
    : _mru(mru)
    {
      set_hit(handler_wrapper<Mru_ppi>);
    }

    void init(Irq_chip_gen *parent, unsigned id)
    {
      check(parent->attach(this, id, false));
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
  { write<Unsigned32>(v.raw, ch * Ch_offset); }

  inline Cfg0 cfg0(unsigned ch) const
  { return Cfg0(read<Unsigned32>(ch * Ch_offset)); }
};

// ------------------------------------------------------------------------
INTERFACE [cpu_virt]:

EXTENSION class Mru
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
};

// ------------------------------------------------------------------------
INTERFACE [!cpu_virt]:

EXTENSION class Mru
{
  // When running on EL1, the MRU is supposed to be accessible...
  class Vmid_guard {
  public:
    Vmid_guard() { /* dummy to prevent warning */ }
    ~Vmid_guard() {}
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

PRIVATE
void
Mru::init(unsigned nr_irqs)
{
  for (unsigned i = 0; i < nr_irqs; i++)
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
}

PUBLIC
bool
Mru::attach(Irq_base *irq, Mword pin, bool init = true) override
{
  bool ret = Irq_chip_gen::attach(irq, pin, init);
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

  auto mm = static_cast<Mru_ch_mode>(m);
  if (mm.set_mb_enable())
    {
      Vmid_guard g;
      unsigned base = pin * Ch_offset;

      Cfg0 c0 = Cfg0(read<Unsigned32>(base + Ch_cfg0));
      c0.mbe() = mm.mbe();
      write<Unsigned32>(c0.raw, base + Ch_cfg0);
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
bool
Mru::set_cpu(Mword, Cpu_number) override
{
  // single core / AMP platform
  return false;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
char const *
Mru::chip_type() const override
{ return "MRU"; }
