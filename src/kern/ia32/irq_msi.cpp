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

  unsigned nr_pins() const override { return Irq_chip_ia32::nr_pins(); }
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
Irq_chip_msi::attach(Irq_base *irq, Mword pin, bool init = true) override
{ return valloc<Irq_chip_msi>(irq, pin, 0, init); }

PUBLIC
void
Irq_chip_msi::detach(Irq_base *irq) override
{
  extern char entry_int_apic_ignore[];
  //Mword n = irq->pin();
  // hm: no way to mask an MSI: mask(n);
  vfree(irq, &entry_int_apic_ignore);
  Irq_chip_icu::detach(irq);
}

PUBLIC
int
Irq_chip_msi::msi_info(Mword pin, Unsigned64, Irq_mgr::Msi_info *info)
{
  // the requester ID does not matter, we cannot verify without IRQ remapping
  if (pin >= _pins)
    return -L4_err::ERange;

  info->data = vector(pin) | (1 << 14);
  Unsigned32 aid{cxx::int_value<Cpu_phys_id>(Apic::apic.current()->cpu_id())};
  info->addr = 0xfee00000 | (aid << 12);

  return 0;
}

PUBLIC
int
Irq_chip_msi::set_mode(Mword, Mode) override
{ return 0; }

PUBLIC
bool
Irq_chip_msi::is_edge_triggered(Mword) const override
{ return true; }

PUBLIC
void
Irq_chip_msi::set_cpu(Mword, Cpu_number) override
{}

PUBLIC
void
Irq_chip_msi::mask(Mword) override
{}

PUBLIC
void
Irq_chip_msi::ack(Mword) override
{ ::Apic::irq_ack(); }

PUBLIC
void
Irq_chip_msi::mask_and_ack(Mword) override
{ ::Apic::irq_ack(); }

PUBLIC
void
Irq_chip_msi::unmask(Mword) override
{}

PUBLIC inline explicit
Irq_mgr_msi::Irq_mgr_msi(Irq_mgr *o) : _orig(o) {}

PUBLIC
Irq_mgr::Chip_pin
Irq_mgr_msi::chip_pin(Mword gsi) const override
{
  if (gsi & Msi_bit)
    return Chip_pin(&_chip, gsi & ~Msi_bit);

  return _orig->chip_pin(gsi);
}

PUBLIC
unsigned
Irq_mgr_msi::nr_gsis() const override
{ return _orig->nr_gsis(); }

PUBLIC
unsigned
Irq_mgr_msi::nr_msis() const override
{ return _chip.nr_pins(); }

PUBLIC
int
Irq_mgr_msi::msi_info(Mword msi, Unsigned64 src, Msi_info *info) const override
{
  if (msi & Msi_bit)
    return _chip.msi_info(msi & ~Msi_bit, src, info);

  return -L4_err::ERange;
}

PUBLIC
Mword
Irq_mgr_msi::legacy_override(Mword isa_pin) override
{
  if (isa_pin & 0x80000000)
    return isa_pin;

  return _orig->legacy_override(isa_pin);
}

PUBLIC static FIASCO_INIT
void
Irq_mgr_msi::init()
{
  if (Irq_mgr::mgr && Irq_mgr::mgr->nr_msis() > 0)
    return;

  Irq_mgr_msi *m;
  Irq_mgr::mgr = m =  new Boot_object<Irq_mgr_msi>(Irq_mgr::mgr);
  printf("Enable MSI support: chained IRQ mgr @ %p\n",
         static_cast<void *>(m->_orig));
}

STATIC_INITIALIZE(Irq_mgr_msi);

IMPLEMENTATION [debug]:

PUBLIC inline
char const *
Irq_chip_msi::chip_type() const override
{ return "MSI"; }
