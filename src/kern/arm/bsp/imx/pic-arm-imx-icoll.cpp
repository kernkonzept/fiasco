// ---------------------------------------------------------------------
INTERFACE [arm && imx28]:

#include "initcalls.h"
#include "kmem.h"

class Irq_base;

// ---------------------------------------------------------------------
IMPLEMENTATION [arm && imx28]:

#include <cassert>
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"

class Irq_chip_imx_icoll : public Irq_chip_gen
{
private:
  enum
  {
    HW_ICOLL_VECTOR         =   0x0,
    HW_ICOLL_LEVELACK       =  0x10,
    HW_ICOLL_CTRL           =  0x20,
    HW_ICOLL_INTERRUPT0     = 0x120,
    HW_ICOLL_INTERRUPT0_SET = 0x124,
    HW_ICOLL_INTERRUPT0_CLR = 0x128,

    HW_ICOLL_CTRL_IRQ_FINAL_ENABLE = 1 << 16,

    HW_ICOLL_INTERRUPT_ENABLE      = 1 << 2,

    PRIO_LEVEL = 0,
  };

  Register_block<32> _reg;

public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
  void ack(Mword) { _reg[HW_ICOLL_LEVELACK] = 1 << PRIO_LEVEL; }
};

PUBLIC
void
Irq_chip_imx_icoll::mask(Mword irq)
{
  assert(cpu_lock.test());
  _reg[HW_ICOLL_INTERRUPT0_CLR + irq * 0x10] = HW_ICOLL_INTERRUPT_ENABLE;
}

PUBLIC
void
Irq_chip_imx_icoll::mask_and_ack(Mword irq)
{
  assert(cpu_lock.test());
  mask(irq);
  ack(irq);
}

PUBLIC
void
Irq_chip_imx_icoll::unmask(Mword irq)
{
  assert (cpu_lock.test());
  _reg[HW_ICOLL_INTERRUPT0_SET + irq * 0x10] = HW_ICOLL_INTERRUPT_ENABLE;
}

PUBLIC inline
Irq_chip_imx_icoll::Irq_chip_imx_icoll()
: Irq_chip_gen(128), _reg(Kmem::mmio_remap(Mem_layout::Pic_phys_base))
{
  _reg[HW_ICOLL_CTRL] = 0;

  for (unsigned i = 0; i < 128; ++i)
    _reg[HW_ICOLL_INTERRUPT0 + i * 0x10] = PRIO_LEVEL; // Normal interrupt

  _reg[HW_ICOLL_CTRL] = HW_ICOLL_CTRL_IRQ_FINAL_ENABLE;
}

static Static_object<Irq_mgr_single_chip<Irq_chip_imx_icoll> > mgr;


PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC inline
Unsigned32 Irq_chip_imx_icoll::pending()
{
  return _reg[HW_ICOLL_VECTOR] >> 2;
}

PUBLIC inline NEEDS[Irq_chip_imx_icoll::pending]
void
Irq_chip_imx_icoll::irq_handler()
{
  Unsigned32 p = pending();
  _reg[HW_ICOLL_VECTOR] = p; // write anything
  handle_irq<Irq_chip_imx_icoll>(p, 0);
}

extern "C"
void irq_handler()
{ mgr->c.irq_handler(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && imx]:

PUBLIC
char const *
Irq_chip_imx_icoll::chip_type() const
{ return "i.MX-icoll IRQ"; }
