INTERFACE [arm && pf_integrator]:

#include "initcalls.h"

// ---------------------------------------------------------------------
IMPLEMENTATION [arm && pf_integrator]:

#include "assert.h"
#include "initcalls.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"
#include "kmem.h"

class Irq_chip_arm_integr : public Irq_chip_gen, Mmio_register_block
{
private:
  enum
  {
    IRQ_STATUS       = 0x00,
    IRQ_ENABLE_SET   = 0x08,
    IRQ_ENABLE_CLEAR = 0x0c,

    FIQ_ENABLE_CLEAR = 0x2c,

    PIC_START = 0,
    PIC_END   = 31,
  };

public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
  void ack(Mword) { /* ack is empty */ }
};

PUBLIC
Irq_chip_arm_integr::Irq_chip_arm_integr()
: Irq_chip_gen(32),
  Mmio_register_block(Kmem::mmio_remap(Mem_layout::Pic_phys_base))
{
  write<Mword>(0xffffffff, IRQ_ENABLE_CLEAR);
  write<Mword>(0xffffffff, FIQ_ENABLE_CLEAR);
}

PUBLIC
void
Irq_chip_arm_integr::mask(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq - PIC_START), IRQ_ENABLE_CLEAR);
}

PUBLIC
void
Irq_chip_arm_integr::mask_and_ack(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq - PIC_START), IRQ_ENABLE_CLEAR);
  // ack is empty
}

PUBLIC
void
Irq_chip_arm_integr::unmask(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq - PIC_START), IRQ_ENABLE_SET);
}

static Static_object<Irq_mgr_single_chip<Irq_chip_arm_integr> > mgr;

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC inline
Unsigned32 Irq_chip_arm_integr::pending()
{
  return read<Mword>(IRQ_STATUS);
}

extern "C"
void irq_handler()
{ mgr->c.handle_multi_pending<Irq_chip_arm_integr>(0); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_integrator && arm_em_tz]:

#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && pf_integrator]:

PUBLIC
char const *
Irq_chip_arm_integr::chip_type() const
{ return "Integrator"; }
