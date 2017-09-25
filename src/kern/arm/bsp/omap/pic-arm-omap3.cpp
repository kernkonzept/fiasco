INTERFACE [arm && omap3]:

#include "initcalls.h"
#include "kmem.h"

class Irq_base;

EXTENSION class Pic
{
public:
};

INTERFACE [arm && omap3_35x]: //-------------------------------------------

EXTENSION class Pic
{
public:
  enum { Num_irqs                 = 96, };
};

INTERFACE [arm && omap3_am33xx]: //----------------------------------------

EXTENSION class Pic
{
public:
  enum { Num_irqs                 = 128, };
};

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && omap3]:

#include "assert.h"
#include "config.h"
#include "io.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"

#include <cstdio>

class Irq_chip_arm_omap3 : public Irq_chip_gen, private Mmio_register_block
{
private:
  enum
  {
    INTCPS_SYSCONFIG         = 0x010,
    INTCPS_SYSSTATUS         = 0x014,
    INTCPS_CONTROL           = 0x048,
    INTCPS_TRESHOLD          = 0x068,
    INTCPS_ITRn_base         = 0x080,
    INTCPS_MIRn_base         = 0x084,
    INTCPS_MIR_CLEARn_base   = 0x088,
    INTCPS_MIR_SETn_base     = 0x08c,
    INTCPS_ISR_SETn_base     = 0x090,
    INTCPS_ISR_CLEARn_base   = 0x094,
    INTCPS_PENDING_IRQn_base = 0x098,
    INTCPS_ILRm_base         = 0x100,
  };
public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
};

PUBLIC inline
Irq_chip_arm_omap3::Irq_chip_arm_omap3()
: Irq_chip_gen(Pic::Num_irqs),
  Mmio_register_block(Kmem::mmio_remap(Mem_layout::Intc_phys_base))
{
  // Reset
  write<Mword>(2, INTCPS_SYSCONFIG);
  while (!read<Mword>(INTCPS_SYSSTATUS))
    ;

  // auto-idle
  write<Mword>(1, INTCPS_SYSCONFIG);

  // disable treshold
  write<Mword>(0xff, INTCPS_TRESHOLD);

  // set priority for each interrupt line, lets take 0x20
  // setting bit0 to 0 means IRQ (1 would mean FIQ)
  for (int m = 0; m < Pic::Num_irqs; ++m)
    write<Mword>(0x20 << 2, INTCPS_ILRm_base + (4 * m));

  // mask all interrupts
  for (int n = 0; n < 3; ++n)
    write<Mword>(0xffffffff, INTCPS_MIR_SETn_base + 0x20 * n);
}

PUBLIC
void
Irq_chip_arm_omap3::mask(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq & 31), INTCPS_MIR_SETn_base + (irq & 0xe0));
}

PUBLIC
void
Irq_chip_arm_omap3::mask_and_ack(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq & 31), INTCPS_MIR_SETn_base + (irq & 0xe0));
  write<Mword>(1, INTCPS_CONTROL);
}

PUBLIC
void
Irq_chip_arm_omap3::ack(Mword irq)
{
  (void)irq;
  write<Mword>(1, INTCPS_CONTROL);
}

PUBLIC
void
Irq_chip_arm_omap3::unmask(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq & 31), INTCPS_MIR_CLEARn_base + (irq & 0xe0));
}

static Static_object<Irq_mgr_single_chip<Irq_chip_arm_omap3> > mgr;

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC inline
Unsigned32 Irq_chip_arm_omap3::pending()
{
  for (int n = 0; n < (Pic::Num_irqs >> 5); ++n)
    {
      unsigned long x = read<Mword>(INTCPS_PENDING_IRQn_base + 0x20 * n);
      for (int i = 0; i < 32; ++i)
        if (x & (1 << i))
          return i + n * 32;
    }
  return 0;
}

extern "C"
void irq_handler()
{
  Unsigned32 i;
  while ((i = mgr->c.pending()))
    mgr->c.handle_irq<Irq_chip_arm_omap3>(i, 0);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && omap3 && arm_em_tz]:

#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}

//-------------------------------------------------------------------------
IMPLEMENTATION [debug && omap3]:

PUBLIC
char const *
Irq_chip_arm_omap3::chip_type() const
{ return "HW OMAP3 IRQ"; }
