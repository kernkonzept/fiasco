INTERFACE [arm && pf_rpi]:

#include "initcalls.h"
#include "irq_chip_generic.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && !pic_gic]:

#include "arithmetic.h"
#include "assert.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"

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

  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  bool set_cpu(Mword, Cpu_number) override { return false; }
  void ack(Mword) override { /* ack is empty */ }
};

PUBLIC
Irq_chip_bcm::Irq_chip_bcm()
: Irq_chip_gen(96),
  Mmio_register_block(Kmem_mmio::map(Mem_layout::Pic_phys_base, 0x100))
{
  write<Unsigned32>(~0U, Disable_Basic_IRQs);
  write<Unsigned32>(~0U, Disable_IRQs_1);
  write<Unsigned32>(~0U, Disable_IRQs_2);
}

PUBLIC
void
Irq_chip_bcm::mask(Mword irq) override
{
  assert(cpu_lock.test());
  write<Unsigned32>(1 << (irq & 0x1f), Disable_IRQs_1 + ((irq & 0x60) >> 3));
}

PUBLIC
void
Irq_chip_bcm::mask_and_ack(Mword irq) override
{
  mask(irq);
  // ack is empty
}

PUBLIC
void
Irq_chip_bcm::unmask(Mword irq) override
{
  assert(cpu_lock.test());
  write<Unsigned32>(1 << (irq & 0x1f), Enable_IRQs_1 + ((irq & 0x60) >> 3));
}

static Static_object<Irq_mgr_single_chip<Irq_chip_bcm> > mgr;


PUBLIC
void
Irq_chip_bcm::irq_handler()
{
  for (;;)
    {
      unsigned b = 64;
      Unsigned32 p = read<Unsigned32>(Irq_basic_pending);

      if (p & 0x100)
        {
          b = 0;
          p = read<Unsigned32>(Irq_pending_1);
        }
      else if (p & 0x200)
        {
          b = 32;
          p = read<Unsigned32>(Irq_pending_2);
        }
      else if (p)
        {
          unsigned m = p & 0x1ffc00;
          if (m)
            {
              m >>= 10;
              char map[11] = { 7, 9, 10, 18, 19, 53, 54, 55, 56, 57, 62 };

              handle_irq<Irq_chip_bcm>(map[cxx::log2u(m)], 0);
              continue;
            }
        }

      if (p)
        handle_irq<Irq_chip_bcm>(b + cxx::log2u(p), 0);
      else
        return;
    }
}

PUBLIC static
void
Pic::handle_irq()
{
  mgr->c.irq_handler();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && !64bit && !pf_rpi_rpi1 && !pf_rpi_rpizw]:

#include "arm_control.h"

PRIVATE static FIASCO_INIT
void Pic::arm_control_init()
{ Arm_control::init(); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && (64bit || pf_rpi_rpi1 || pf_rpi_rpizw)]:

PRIVATE static FIASCO_INIT
void Pic::arm_control_init()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && (pf_rpi_rpi1 || pf_rpi_rpizw)]:

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && (pf_rpi_rpi2 || pf_rpi_rpi3)]:

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
  arm_control_init();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && !pic_gic && mp]:

PUBLIC static
void
Pic::init_ap(Cpu_number, bool)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_rpi && !pic_gic && arm_em_tz]:

#include <cstdio>

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  printf("%s(%d, %x): Not implemented\n", __func__, group32num, val);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

#include "boot_alloc.h"
#include "gic_v2.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v2> M;

  M *m = new Boot_object<M>(Kmem_mmio::map(Mem_layout::Gic_cpu_phys_base,
                                           Gic_cpu_v2::Size),
                            Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size));
  gic = &m->c;
  Irq_mgr::mgr = m;

  arm_control_init();
}

//-------------------------------------------------------------------------
IMPLEMENTATION [debug && pf_rpi && !pic_gic]:

PUBLIC
char const *
Irq_chip_bcm::chip_type() const override
{ return "Raspberry Pi"; }
