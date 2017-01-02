INTERFACE [arm && pf_xscale]: // -------------------------------------

#include "initcalls.h"
#include "kmem.h"

EXTENSION class Pic
{
public:
  typedef Mword Status;
  enum {
    ICIP = 0x000000,
    ICMR = 0x000004,
    ICLR = 0x000008,
    ICCR = 0x000014,
    ICFP = 0x00000c,
    ICPR = 0x000010,
  };
};

INTERFACE [arm && pf_sa1100]: // ----------------------------------

#include "initcalls.h"
#include "kmem.h"

EXTENSION class Pic
{
public:
  typedef Mword Status;
  enum {
    ICIP = 0x00000,
    ICMR = 0x00004,
    ICLR = 0x00008,
    ICCR = 0x0000c,
    ICFP = 0x00010,
    ICPR = 0x00020,
  };
};

// -------------------------------------------------------------
IMPLEMENTATION [arm && (pf_xscale || pf_sa1100)]:

#include "assert.h"
#include "config.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"

class Chip : public Irq_chip_gen, private Mmio_register_block
{
public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
  void ack(Mword) { /* ack is empty */ }
};

PUBLIC inline
Chip::Chip()
: Irq_chip_gen(32),
  Mmio_register_block(Kmem::mmio_remap(Mem_layout::Pic_phys_base))
{
  // only unmasked interrupts wakeup from idle
  write<Mword>(0x01, Pic::ICCR);
  // mask all interrupts
  write<Mword>(0x00, Pic::ICMR);
  // all interrupts are IRQ's (no FIQ)
  write<Mword>(0x00, Pic::ICLR);
}

PUBLIC
void
Chip::mask(Mword irq)
{
  assert(cpu_lock.test());
  modify<Mword>(0, 1 << irq, Pic::ICMR);
}

PUBLIC
void
Chip::mask_and_ack(Mword irq)
{
  assert (cpu_lock.test());
  modify<Mword>(0, 1 << irq, Pic::ICMR);
  // ack is empty
}

PUBLIC
void
Chip::unmask(Mword irq)
{
  modify<Mword>(1 << irq, 0, Pic::ICMR);
}

// for JDB only
PUBLIC inline
Mword
Chip::disable_all_save()
{
  Mword s = read<Mword>(Pic::ICMR);
  write<Mword>(0, Pic::ICMR);
  return s;
}

// for JDB only
PUBLIC inline
void
Chip::restore_all(Mword s)
{
  write(s, Pic::ICMR);
}


static Static_object<Irq_mgr_single_chip<Chip> > mgr;

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

// for JDB only
PUBLIC
Pic::Status Pic::disable_all_save()
{ return mgr->c.disable_all_save(); }

PUBLIC
void
Pic::restore_all(Status s)
{
  mgr->c.restore_all(s);
}

PUBLIC inline
Unsigned32 Chip::pending()
{
  return read<Unsigned32>(Pic::ICIP);
}

extern "C"
void irq_handler()
{
  Unsigned32 i = mgr->c.pending();
  if (i)
    mgr->c.handle_irq<Chip>(i, 0);
}

// -------------------------------------------------------------
IMPLEMENTATION [arm && debug && (pf_sa1100 || pf_xscale)]:

PUBLIC
char const *
Chip::chip_type() const
{ return "HW PXA/SA IRQ"; }
