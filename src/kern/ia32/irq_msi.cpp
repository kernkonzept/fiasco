IMPLEMENTATION:

#include "idt.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "irq_chip_ia32.h"

#include "apic.h"
#include "static_init.h"
#include "boot_alloc.h"

class Irq_chip_msi : public Irq_chip_icu, private Irq_chip_ia32
{
  friend class Irq_chip_ia32;
public:
  // this is somehow arbitrary
  enum { Max_msis = Int_vector_allocator::End - Int_vector_allocator::Base - 0x8 };
  Irq_chip_msi() : Irq_chip_ia32(Max_msis) {}

  unsigned nr_irqs() const override { return Irq_chip_ia32::nr_irqs(); }
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }
};

class Irq_mgr_msi : public Irq_mgr
{
private:
  mutable Irq_chip_msi _chip;
  Irq_mgr *_orig;
};

PUBLIC
bool
Irq_chip_msi::alloc(Irq_base *irq, Mword pin) override
{ return valloc<Irq_chip_msi>(irq, pin, 0); }

PUBLIC
void
Irq_chip_msi::unbind(Irq_base *irq) override
{
  extern char entry_int_apic_ignore[];
  //Mword n = irq->pin();
  // hm: no way to mask an MSI: mask(n);
  vfree(irq, &entry_int_apic_ignore);
  Irq_chip_icu::unbind(irq);
}

PUBLIC
int
Irq_chip_msi::msg(Mword pin, Unsigned64, Irq_mgr::Msi_info *inf)
{
  // the requester ID does not matter, we cannot verify
  // without IRQ remapping
  if (pin >= _irqs)
    return -L4_err::ERange;

  inf->data = _entry[pin].vector() | (1 << 14);
  inf->addr = 0xfee00000;

  return 0;
}

PUBLIC int
Irq_chip_msi::set_mode(Mword, Mode) override
{ return 0; }

PUBLIC bool
Irq_chip_msi::is_edge_triggered(Mword) const override
{ return true; }

PUBLIC void
Irq_chip_msi::set_cpu(Mword, Cpu_number) override
{}

PUBLIC void
Irq_chip_msi::mask(Mword) override
{}

PUBLIC void
Irq_chip_msi::ack(Mword) override
{ ::Apic::irq_ack(); }

PUBLIC void
Irq_chip_msi::mask_and_ack(Mword) override
{ ::Apic::irq_ack(); }

PUBLIC void
Irq_chip_msi::unmask(Mword) override
{}


PUBLIC inline explicit
Irq_mgr_msi::Irq_mgr_msi(Irq_mgr *o) : _orig(o) {}

PUBLIC Irq_mgr::Irq
Irq_mgr_msi::chip(Mword irq) const override
{
  if (irq & 0x80000000)
    return Irq(&_chip, irq & ~0x80000000);
  else
    return _orig->chip(irq);
}

PUBLIC
unsigned
Irq_mgr_msi::nr_irqs() const override
{ return _orig->nr_irqs(); }

PUBLIC
unsigned
Irq_mgr_msi::nr_msis() const override
{ return _chip.nr_irqs(); }

PUBLIC
int
Irq_mgr_msi::msg(Mword irq, Unsigned64 src, Msi_info *inf) const override
{
  if (irq & 0x80000000)
    return _chip.msg(irq & ~0x80000000, src, inf);
  else
    return -L4_err::ERange;
}

PUBLIC unsigned
Irq_mgr_msi::legacy_override(Mword irq) override
{
  if (irq & 0x80000000)
    return irq;
  else
    return _orig->legacy_override(irq);
}


PUBLIC static FIASCO_INIT
void
Irq_mgr_msi::init()
{
  if (Irq_mgr::mgr && Irq_mgr::mgr->nr_msis() > 0)
    return;

  Irq_mgr_msi *m;
  Irq_mgr::mgr = m =  new Boot_object<Irq_mgr_msi>(Irq_mgr::mgr);
  printf("Enable MSI support: chained IRQ mgr @ %p\n", m->_orig);
}

STATIC_INITIALIZE(Irq_mgr_msi);

IMPLEMENTATION [debug]:

PUBLIC inline
char const *
Irq_chip_msi::chip_type() const override
{ return "MSI"; }

