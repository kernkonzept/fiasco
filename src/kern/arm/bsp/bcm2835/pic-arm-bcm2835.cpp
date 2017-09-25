INTERFACE [arm && bcm2835]:

#include "initcalls.h"

// ---------------------------------------------------------------------
IMPLEMENTATION [arm && bcm2835]:

#include "assert.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"
#include "kmem.h"

class Irq_chip_bcm : public Irq_chip_gen, Mmio_register_block
{
public:
  enum
  {
    Irq_basic_pending  = 0x0,
    Irq_pending_1      = 0x4,
    Irq_pending_2      = 0x8,
    Fiq_control        = 0xc,
    Enable_IRQs_1      = 0x10,
    Enable_IRQs_2      = 0x14,
    Enable_Basic_IRQs  = 0x18,
    Disable_IRQs_1     = 0x1c,
    Disable_IRQs_2     = 0x20,
    Disable_Basic_IRQs = 0x24,
  };

  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
  void ack(Mword) { /* ack is empty */ }
};

PUBLIC
Irq_chip_bcm::Irq_chip_bcm()
: Irq_chip_gen(96),
  Mmio_register_block(Kmem::mmio_remap(Mem_layout::Pic_phys_base))
{
  write<Mword>(~0UL, Disable_Basic_IRQs);
  write<Mword>(~0UL, Disable_IRQs_1);
  write<Mword>(~0UL, Disable_IRQs_2);
}

PUBLIC
void
Irq_chip_bcm::mask(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq & 0x1f), Disable_IRQs_1 + ((irq & 0x60) >> 3));
}

PUBLIC
void
Irq_chip_bcm::mask_and_ack(Mword irq)
{
  mask(irq);
  // ack is empty
}

PUBLIC
void
Irq_chip_bcm::unmask(Mword irq)
{
  assert(cpu_lock.test());
  write<Mword>(1 << (irq & 0x1f), Enable_IRQs_1 + ((irq & 0x60) >> 3));
}

static Static_object<Irq_mgr_single_chip<Irq_chip_bcm> > mgr;

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC
void
Irq_chip_bcm::irq_handler()
{
  while (1)
    {
      unsigned b = 64;
      Mword p = read<Mword>(Irq_basic_pending);

      if (p & 0x100)
        {
          b = 0;
          p = read<Mword>(Irq_pending_1);
        }
      else if (p & 0x200)
        {
          b = 32;
          p = read<Mword>(Irq_pending_2);
        }
      else if (p)
        {
          unsigned m = p & 0x1ffc00;
          if (m)
            {
              m >>= 10;
              char map[11] = { 7, 9, 10, 18, 19, 53, 54, 55, 56, 57, 62 };

              handle_irq<Irq_chip_bcm>(map[31 - __builtin_clz(m)], 0);
              continue;
            }
        }

      if (p)
        handle_irq<Irq_chip_bcm>(b + 31 - __builtin_clz(p), 0);
      else
        return;
    }
}

extern "C"
void irq_handler()
{ mgr->c.irq_handler(); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && bcm2835 && arm_em_tz]:

#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && bcm2835]:

PUBLIC
char const *
Irq_chip_bcm::chip_type() const
{ return "BCM2835"; }
