INTERFACE [arm && pf_kirkwood]:

#include "initcalls.h"

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pf_kirkwood]:

#include "assert.h"
#include "config.h"
#include "io.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "kmem.h"
#include "mmio_register_block.h"

class Irq_chip_kirkwood : public Irq_chip_gen, private Mmio_register_block
{
private:
  enum
  {
    Main_Irq_cause_low_reg     = 0x20200,
    Main_Irq_mask_low_reg      = 0x20204,
    Main_Fiq_mask_low_reg      = 0x20208,
    Endpoint_irq_mask_low_reg  = 0x2020c,
    Main_Irq_cause_high_reg    = 0x20210,
    Main_Irq_mask_high_reg     = 0x20214,
    Main_Fiq_mask_high_reg     = 0x20218,
    Endpoint_irq_mask_high_reg = 0x2021c,

    Bridge_int_num = 1,
  };

public:
  Irq_chip_kirkwood()
  : Irq_chip_gen(64),
    Mmio_register_block(Kmem::mmio_remap(Mem_layout::Pic_phys_base))
  {
    // Disable all interrupts
    write<Unsigned32>(0U, Main_Irq_mask_low_reg);
    write<Unsigned32>(0U, Main_Fiq_mask_low_reg);
    write<Unsigned32>(0U, Main_Irq_mask_high_reg);
    write<Unsigned32>(0U, Main_Fiq_mask_high_reg);
    // enable bridge (chain) IRQ
    modify<Unsigned32>(1 << Bridge_int_num, 0, Main_Irq_mask_low_reg);
  }

  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
  void ack(Mword) { /* ack is empty */ }
};

PUBLIC
void
Irq_chip_kirkwood::mask(Mword irq)
{
  assert (cpu_lock.test());
  modify<Unsigned32>(0, 1 << (irq & 0x1f),
                     Main_Irq_mask_low_reg + ((irq & 0x20) >> 1));
}

PUBLIC
void
Irq_chip_kirkwood::mask_and_ack(Mword irq)
{
  assert(cpu_lock.test());
  mask(irq);
  // ack is empty
}

PUBLIC
void
Irq_chip_kirkwood::unmask(Mword irq)
{
  assert(cpu_lock.test());
  modify<Unsigned32>(1 << (irq & 0x1f), 0,
                     Main_Irq_mask_low_reg + ((irq & 0x20) >> 1));
}

static Static_object<Irq_mgr_single_chip<Irq_chip_kirkwood> > mgr;

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

PUBLIC inline
Unsigned32
Irq_chip_kirkwood::pending()
{
  Unsigned32 v;

  v = read<Unsigned32>(Main_Irq_cause_low_reg);
  if (v & 1)
    {
      v = read<Unsigned32>(Main_Irq_cause_high_reg);
      for (int i = 1; i < 32; ++i)
	if ((1 << i) & v)
	  return 32 + i;
    }
  for (int i = 1; i < 32; ++i)
    if ((1 << i) & v)
      return i;

  return 64;
}

extern "C"
void irq_handler()
{
  Unsigned32 i;
  while ((i = mgr->c.pending()) < 64)
    mgr->c.handle_irq<Irq_chip_kirkwood>(i, 0);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && pf_kirkwood]:

PUBLIC
char const *
Irq_chip_kirkwood::chip_type() const
{ return "HW Kirkwood IRQ"; }
