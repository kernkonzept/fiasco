INTERFACE:

#include "mmio_register_block.h"

class Gt64120
{
public:
  enum class R
  {
    Cpu_iface_conf         = 0x000,
    Pci0_sync              = 0x0c0, // ro
    Pci1_sync              = 0x0c8, // ro

    Multi_gt               = 0x120,

    Pci0_command           = 0xc00,
    Pci0_to_rtry           = 0xc04,
    Int_cause              = 0xc18,
    Cpu_int_mask           = 0xc1c,
    Pci0_int_cause_mask    = 0xc24,
    Int_cause_hi           = 0xc98,
    Cpu_int_mask_hi        = 0xc9c,
    Pci0_int_cause_mask_hi = 0xca4,
    Pci1_cfg_addr          = 0xcf0,
    Pci1_cfg_data          = 0xcf4,
    Pci0_cfg_addr          = 0xcf8,
    Pci0_cfg_data          = 0xcfc,
    Pci1_iack              = 0xc30, // ro
    Pci0_iack              = 0xc34, // ro
  };

  typedef Register_block<32, R> Gt_regs;

  Mmio_register_block const *pci_io() const { return &_pci_io; }
  Gt_regs const &gt_regs() const { return _gt_regs; }

private:
  void irq_init();
  Gt_regs _gt_regs;
  Mmio_register_block _pci_io;
};

// ---------------------------------------------------------------
INTERFACE [gt64120_irq]:

#include "irq_chip_generic.h"
#include "spin_lock.h"

EXTENSION class  Gt64120 : public Irq_chip_gen
{
private:
  Spin_lock<> _irq_lock;
};

// ---------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT_DEFAULT inline void Gt64120::irq_init() {}

PUBLIC
Gt64120::Gt64120(Address virt_gt_regs, Address virt_pci_io)
: _gt_regs(virt_gt_regs),
  _pci_io(virt_pci_io)
{
  irq_init();
}

// ---------------------------------------------------------------
IMPLEMENTATION [gt64120_irq]:

IMPLEMENT inline
void
Gt64120::irq_init()
{
  Irq_chip_gen::init(29);
  _irq_lock = Spin_lock<>::Unlocked;
  _gt_regs[R::Cpu_int_mask] = 0;
  _gt_regs[R::Int_cause] = 0;
}

PUBLIC
void
Gt64120::unmask(Mword pin) override
{
  auto g = lock_guard(_irq_lock);
  _gt_regs[R::Cpu_int_mask].set(1UL << (pin + 1));
}

PUBLIC
void
Gt64120::mask(Mword pin) override
{
  auto g = lock_guard(_irq_lock);
  _gt_regs[R::Cpu_int_mask].clear(1UL << (pin + 1));
}

PUBLIC
void
Gt64120::mask_and_ack(Mword pin) override
{
  auto g = lock_guard(_irq_lock);
  _gt_regs[R::Cpu_int_mask].clear(1UL << (pin + 1));
  // NOTE: this is according to the spec, writing a 0
  // for the IRQ to ack and all ones for the other bits.
  // However, Linux uses read-modify-write for that, what
  // creates a reace condition IMHO.
  _gt_regs[R::Int_cause] = ~(1UL << (pin + 1));
}

PUBLIC
void
Gt64120::ack(Mword pin) override
{
  // This is a simple write, so no lock used

  // NOTE: this is according to the spec, writing a 0
  // for the IRQ to ack and all ones for the other bits.
  // However, Linux uses read-modify-write for that, what
  // creates a reace condition IMHO.
  _gt_regs[R::Int_cause] = ~(1UL << (pin + 1));
}

PUBLIC
int
Gt64120::set_mode(Mword, Mode) override
{ return 0; }

PUBLIC
bool
Gt64120::is_edge_triggered(Mword) const override
{ return false; }

PUBLIC inline
void
Gt64120::set_cpu(Mword, Cpu_number) override
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [gt64120_irq && debug]:

PUBLIC
char const *
Gt64120::chip_type() const override
{ return "GT64120"; }

