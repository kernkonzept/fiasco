INTERFACE [sparc]:

#include "initcalls.h"

IMPLEMENTATION[sparc]:

#include <cstdio>

#include "assert.h"
#include "initcalls.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmio_register_block.h"
#include "kmem.h"

class Irq_chip_sparc : public Irq_chip_gen, Mmio_register_block
{
private:
  enum
  {
    ILEVEL         = 0x0,
    IPEND          = 0x4,
    IFORCE         = 0x8,
    ICLEAR         = 0xc,
    MPSTAT         = 0x10,
    BRDCST         = 0x14,
    ERRSTAT        = 0x18,
    PIMASK         = 0x40,
    PIFORCE        = 0x80,
    PEXRACK        = 0xc0,
  };

public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
};

PUBLIC
Irq_chip_sparc::Irq_chip_sparc()
: Irq_chip_gen(16),
  Mmio_register_block(Kmem::mmio_remap(0x80000200))
{
  Unsigned32 mpstat = r<32>(MPSTAT);

  printf("MP Info: %d cores, BA=%d ER=%d EIRQ=%d PWRDOWN-stat=%x [%08x]\n",
         (mpstat >> 28) + 1, (mpstat >> 27) & 1, (mpstat >> 26) & 1,
         (mpstat >> 16) & 0xf, mpstat & 0xffff, mpstat);
}

PRIVATE static inline
unsigned
Irq_chip_sparc::cpu()
{ return 0; }

PUBLIC
void
Irq_chip_sparc::mask(Mword irq)
{
  assert(cpu_lock.test());
  printf("IRQ-mask: irq=%ld\n", irq);

  r<32>(PIMASK + 4 * cpu()).clear(1 << irq);
}

PUBLIC
void
Irq_chip_sparc::ack(Mword irq)
{
  r<32>(ICLEAR) = 1 << irq;
}

PUBLIC
void
Irq_chip_sparc::mask_and_ack(Mword irq)
{
  assert(cpu_lock.test());
  printf("IRQ-mask+ack: irq=%ld\n", irq);
  mask(irq);
  ack(irq);
}

PUBLIC
void
Irq_chip_sparc::unmask(Mword irq)
{
  assert(cpu_lock.test());
  printf("IRQ-unmask: irq=%ld\n", irq);
  r<32>(PIMASK + 4 * cpu()).set(1 << irq);
}

PUBLIC inline
Unsigned32 Irq_chip_sparc::pending()
{
  assert(0); // The CPU (QEMU) acks/clears the pending mask immediately
  return r<32>(IPEND);
}

static Static_object<Irq_mgr_single_chip<Irq_chip_sparc> > mgr;

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = mgr.construct();
}

extern "C"
void irq_handler(unsigned irq)
{
  if (0)
    mgr->c.handle_multi_pending<Irq_chip_sparc>(0);

  mgr->c.handle_irq<Irq_chip_sparc>(irq, 0);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [sparc && debug]:

PUBLIC
char const *
Irq_chip_sparc::chip_type() const
{ return "Sparc something fixme"; }
