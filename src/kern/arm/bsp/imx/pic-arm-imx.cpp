// ---------------------------------------------------------------------
INTERFACE [arm && (pf_imx_21 || pf_imx_35)]:

#include "initcalls.h"
#include "kmem_mmio.h"

class Irq_base;

EXTENSION class Pic
{
public:
};

// ---------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_imx_21 || pf_imx_35)]:

#include <cassert>
#include "io.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"

class Irq_chip_arm_imx : public Irq_chip_gen, private Mmio_register_block
{
private:
  enum
  {
    INTCTL      = 0x00,
    NIMASK      = 0x04,
    INTENNUM    = 0x08,
    INTDISNUM   = 0x0c,
    INTENABLEH  = 0x10,
    INTENABLEL  = 0x14,
    INTTYPEH    = 0x18,
    INTTYPEL    = 0x1c,
    NIPRIORITY7 = 0x20,
    NIPRIORITY0 = 0x3c,
    NIVECSR     = 0x40,
    FIVECSR     = 0x44,
    INTSRCH     = 0x48,
    INTSRCL     = 0x4c,
    INTFRCH     = 0x50,
    INTFRCL     = 0x54,
    NIPNDH      = 0x58,
    NIPNDL      = 0x5c,
    FIPNDH      = 0x60,
    FIPNDL      = 0x64,


    INTCTL_FIAD  = 1 << 19, // Fast Interrupt Arbiter Rise ARM Level
    INTCTL_NIAD  = 1 << 20, // Normal Interrupt Arbiter Rise ARM Level
    INTCTL_FIDIS = 1 << 21, // Fast Interrupt Disable
    INTCTL_NIDIS = 1 << 22, // Normal Interrupt Disable
  };
public:
  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}
  void ack(Mword) override { /* ack is empty */ }
};

PUBLIC
void
Irq_chip_arm_imx::mask(Mword irq) override
{
  assert(cpu_lock.test());
  write<Mword>(irq, INTDISNUM); // disable pin
}

PUBLIC
void
Irq_chip_arm_imx::mask_and_ack(Mword irq) override
{
  assert(cpu_lock.test());
  write<Mword>(irq, INTDISNUM); // disable pin
  // ack is empty
}

PUBLIC
void
Irq_chip_arm_imx::unmask(Mword irq) override
{
  assert (cpu_lock.test());
  write<Mword>(irq, INTENNUM);
}

PUBLIC inline
Irq_chip_arm_imx::Irq_chip_arm_imx()
: Irq_chip_gen(64),
  Mmio_register_block(Kmem_mmio::map(Mem_layout::Pic_phys_base, 0x100))
{
  write<Mword>(0,    INTCTL);
  write<Mword>(0x10, NIMASK); // Do not disable any normal interrupts

  write<Mword>(0, INTTYPEH); // All interrupts generate normal interrupts
  write<Mword>(0, INTTYPEL);

  // Init interrupt priorities
  for (int i = 0; i < 8; ++i)
    write<Mword>(0x1111, NIPRIORITY7 + (i * 4)); // low addresses start with 7
}

static Static_object<Irq_mgr_single_chip<Irq_chip_arm_imx> > mgr;


PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC inline
Unsigned32 Irq_chip_arm_imx::pending()
{
  return read<Mword>(NIVECSR) >> 16;
}

PUBLIC inline NEEDS[Irq_chip_arm_imx::pending]
void
Irq_chip_arm_imx::irq_handler()
{
  Unsigned32 p = pending();
  if (EXPECT_TRUE(p != 0xffff))
    handle_irq<Irq_chip_arm_imx>(p, 0);
}

extern "C"
void irq_handler()
{ mgr->c.irq_handler(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && pf_imx]:

PUBLIC
char const *
Irq_chip_arm_imx::chip_type() const override
{ return "HW i.MX IRQ"; }
