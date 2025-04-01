INTERFACE:

#include "i8259.h"
#include "io.h"
#include "irq_chip_ia32.h"
#include "irq_mgr.h"

/**
 * IRQ Chip based on the IA32 legacy PIC.
 *
 * 16 Vectors starting from Base_vector are statically assigned.
 */
class Irq_chip_ia32_pic :
  public Irq_chip_i8259<Io>,
  private Irq_chip_ia32,
  private Irq_mgr
{
  friend class Irq_chip_ia32;
public:
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }

private:
  enum { Base_vector = 0x20 };
};

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>

#include "boot_alloc.h"
#include "cpu_lock.h"
#include "globalconfig.h"
#include "globals.h"
#include "irq_mgr.h"
#include "koptions.h"
#include "pic.h"

PUBLIC
bool
Irq_chip_ia32_pic::attach(Irq_base *irq, Mword irqn, bool init = true) override
{
  // no mor than 16 IRQs
  if (irqn > 0xf)
    return false;

  // PIC uses 16 vectors from Base_vector statically
  unsigned vector = Base_vector + irqn;
  return valloc<Irq_chip_ia32_pic>(irq, irqn, vector, init);
}

PUBLIC
void
Irq_chip_ia32_pic::detach(Irq_base *irq) override
{
  extern char entry_int_pic_ignore[];
  mask(irq->pin());
  vfree(irq, &entry_int_pic_ignore);
  Irq_chip::detach(irq);
}

PRIVATE
Irq_mgr::Chip_pin
Irq_chip_ia32_pic::chip_pin(Mword gsi) const override
{
  if (gsi < 16)
    return Chip_pin(const_cast<Irq_chip_ia32_pic *>(this), gsi);

  return Chip_pin();
}

PUBLIC
unsigned
Irq_chip_ia32_pic::nr_pins() const override
{ return 16; }

PUBLIC
unsigned
Irq_chip_ia32_pic::nr_gsis() const override
{ return 16; }

PUBLIC
unsigned
Irq_chip_ia32_pic::nr_msis() const override
{ return 0; }

PUBLIC
Irq_chip_ia32_pic::Irq_chip_ia32_pic()
: Irq_chip_i8259<Io>(0x20, 0xa0), Irq_chip_ia32(16)
{
  Irq_mgr::mgr = this;
  bool sfn = !Koptions::o()->opt(Koptions::F_nosfn);
  init(Base_vector, sfn,
       Config::Pic_prio_modify
       && int{Config::Scheduler_mode} == Config::SCHED_RTC);

  reserve(2); // reserve cascade irq
  reserve(7); // reserve spurious vect
}
