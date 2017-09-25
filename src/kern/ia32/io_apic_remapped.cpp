INTERFACE:

#include "io_apic.h"
namespace Intel { class Io_mmu; }

class Io_apic_remapped : public Io_apic
{
public:
  Io_apic_remapped(Unsigned64 phys, unsigned gsi_base,
                   Intel::Io_mmu *iommu, Unsigned16 src_id)
  : Io_apic(phys, gsi_base), _iommu(iommu), _src_id(src_id)
  {}

private:
  Intel::Io_mmu *_iommu;
  Unsigned16 _src_id;
};

// -----------------------------------------------------------------
IMPLEMENTATION:

#include "acpi_dmar.h"
#include "intel_iommu.h"

#include "types.h"
#include "idt.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "irq_chip_ia32.h"

#include "apic.h"
#include "boot_alloc.h"
#include "io_apic.h"
#include "cxx/static_vector"
#include "intel_iommu.h"

#include "warn.h"

class Irq_chip_rmsi : public Irq_chip_icu, private Irq_chip_ia32
{
  friend class Irq_chip_ia32;
public:
  // this is somehow arbitrary
  enum { Max_msis = Int_vector_allocator::End - Int_vector_allocator::Base - 0x8 };
  explicit Irq_chip_rmsi(Intel::Io_mmu::Irte volatile *irt)
  : Irq_chip_ia32(Max_msis), _irt(irt)
  {}

  unsigned nr_irqs() const override { return Irq_chip_ia32::nr_irqs(); }
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }

  Intel::Io_mmu::Irte volatile *_irt;
};

class Irq_mgr_rmsi : public Io_apic_mgr
{
public:
  mutable Irq_chip_rmsi _chip;
};

PRIVATE inline NOEXPORT
void
Irq_chip_rmsi::inv_iec(unsigned vect)
{
  for (auto &i: Intel::Io_mmu::iommus)
    if (i.irq_remapping_table() == _irt)
      i.invalidate_iec(vect);
}

PUBLIC
bool
Irq_chip_rmsi::alloc(Irq_base *irq, Mword pin) override
{ return valloc<Irq_chip_rmsi>(irq, pin, 0); }

PUBLIC
void
Irq_chip_rmsi::unbind(Irq_base *irq) override
{
  extern char entry_int_apic_ignore[];
  unsigned vect = _entry[irq->pin()].vector();

  _irt[vect].clear();
  asm volatile ("wbinvd");
  inv_iec(vect);

  vfree(irq, &entry_int_apic_ignore);
  Irq_chip_icu::unbind(irq);
}

PUBLIC
int
Irq_chip_rmsi::msg(Mword pin, Unsigned64 src, Irq_mgr::Msi_info *inf)
{
  if (pin >= _irqs)
    return -L4_err::ERange;

  unsigned vect = _entry[pin].vector();
  if (!vect)
    return 0;

  Intel::Io_mmu::Irte e = _irt[vect];
  bool need_flush = e.present();

  e.fpd() = 1;
  e.vector() = vect;
  e.src_info() = src;

  _irt[vect] = e;
  asm volatile ("wbinvd");

  if (need_flush)
    inv_iec(vect);

  inf->data = 0;
  inf->addr = 0xfee00010 | (vect << 5);

  return 0;
}

PUBLIC int
Irq_chip_rmsi::set_mode(Mword, Mode) override
{ return 0; }

PUBLIC bool
Irq_chip_rmsi::is_edge_triggered(Mword) const override
{ return true; }

PUBLIC void
Irq_chip_rmsi::set_cpu(Mword pin, Cpu_number cpu) override
{
  unsigned vect = _entry[pin].vector();
  if (!vect)
    return;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte e = irte;
  Mword target =  Apic::apic.cpu(cpu)->cpu_id();

  if (target == e.dst_xapic())
    return;

  e.dst_xapic() = target;
  irte = e;
  asm volatile ("wbinvd");

  if (e.present())
    inv_iec(vect);
}

PUBLIC void
Irq_chip_rmsi::mask(Mword pin) override
{
  unsigned vect = _entry[pin].vector();
  if (!vect)
    return;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte e = irte;
  if (!e.present())
    return;

  e.present() = 0;
  irte = e;
  asm volatile ("wbinvd");
  inv_iec(vect);
}

PUBLIC void
Irq_chip_rmsi::ack(Mword) override
{ ::Apic::irq_ack(); }

PUBLIC void
Irq_chip_rmsi::mask_and_ack(Mword pin) override
{
  unsigned vect = _entry[pin].vector();
  if (!vect)
    return;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte e = irte;
  if (!e.present())
    return;

  e.present() = 0;
  irte = e;
  asm volatile ("wbinvd");
  inv_iec(vect);
  ::Apic::irq_ack();
}

PUBLIC void
Irq_chip_rmsi::unmask(Mword pin) override
{
  unsigned vect = _entry[pin].vector();
  if (!vect)
    return;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte e = irte;
  // ignore half initialized MSIs, need to use before unmask
  if (e.vector() != vect)
    return;

  if (e.present())
    return;

  e.present() = 1;
  irte = e;
  asm volatile ("wbinvd");
}


PUBLIC inline explicit
Irq_mgr_rmsi::Irq_mgr_rmsi(Intel::Io_mmu::Irte volatile *irt)
: _chip(irt)
{}

PUBLIC Irq_mgr::Irq
Irq_mgr_rmsi::chip(Mword irq) const override
{
  if (irq & 0x80000000)
    return Irq(&_chip, irq & ~0x80000000);
  else
    return Io_apic_mgr::chip(irq);
}

PUBLIC
unsigned
Irq_mgr_rmsi::nr_irqs() const override
{ return Io_apic_mgr::nr_irqs(); }

PUBLIC
unsigned
Irq_mgr_rmsi::nr_msis() const override
{ return _chip.nr_irqs(); }

PUBLIC
int
Irq_mgr_rmsi::msg(Mword irq, Unsigned64 src, Msi_info *inf) const override
{
  if (irq & 0x80000000)
    return _chip.msg(irq & ~0x80000000, src, inf);
  else
    return -L4_err::ERange;
}

PUBLIC unsigned
Irq_mgr_rmsi::legacy_override(Mword irq) override
{
  if (irq & 0x80000000)
    return irq;
  else
    return Io_apic_mgr::legacy_override(irq);
}


PUBLIC
bool
Io_apic_remapped::alloc(Irq_base *irq, Mword pin) override
{
  unsigned v = valloc<Io_apic_remapped>(irq, pin, 0);

  if (!v)
    return false;

  Io_apic_entry e = read_entry(pin);

  Intel::Io_mmu::Irte i;
  i.present() = 1;
  i.fpd() = 1;
  i.vector() = v;
  i.tm() = e.trigger();
  i.svt() = 1;
  i.sid() = _src_id;
  _iommu->set_irq_mapping(i, v, false);

  e.format() = 1;
  e.vector() = v;
  e.irt_index() = v;
  write_entry(pin, e);
  return true;
}

PUBLIC
void
Io_apic_remapped::unbind(Irq_base *irq) override
{
  unsigned vect = _entry[irq->pin()].vector();
  _iommu->set_irq_mapping(Intel::Io_mmu::Irte(), vect, true);
  Io_apic::unbind(irq);
}

PUBLIC int
Io_apic_remapped::set_mode(Mword pin, Mode mode) override
{
  if (!mode.set_mode())
    return 0;

  Io_apic::set_mode(pin, mode);

  unsigned vect = _entry[pin].vector();
  if (EXPECT_FALSE(vect == 0))
    return 0;

  Intel::Io_mmu::Irte i = _iommu->get_irq_mapping(vect);
  bool tm = to_io_apic_trigger(mode);
  if (tm != i.tm())
    {
      i.tm() = tm;
      _iommu->set_irq_mapping(i, vect, true);
    }

  return 0;
}


PUBLIC void
Io_apic_remapped::set_cpu(Mword pin, Cpu_number cpu) override
{
  unsigned vect = _entry[pin].vector();
  if (!vect)
    return;

  Intel::Io_mmu::Irte e = _iommu->get_irq_mapping(vect);
  Mword target = ::Apic::apic.cpu(cpu)->cpu_id();

  if (e.dst_xapic() == target)
    return;

  e.dst_xapic() = target;
  _iommu->set_irq_mapping(e, vect, true);
}


PUBLIC static FIASCO_INIT
bool
Io_apic_remapped::init_apics()
{
  enum { Print_infos = 0 };

  auto madt = lookup_madt();
  // no MADT -> no IO APIC -> no IRQ remapping
  if (!madt)
    return false;

  // no DMAR or no INTR_REMAP supported -> fall back to normal IO APIC
  if (!Intel::Io_mmu::dmar_flags.intr_remap())
    return Io_apic::init_scan_apics();

  enum
  {
    IRT_size = 8,
    Num_irtes = 1 << IRT_size
  };

  typedef Intel::Io_mmu::Irte Irte;
  Irte *irt = new (Kmem_alloc::allocator()->unaligned_alloc(Num_irtes * sizeof(Irte)))
                  Irte[Num_irtes];
  if (!irt)
    panic("IOMMU: could not allocate interrupt remapping table\n");

  Address irt_pa = Kmem::virt_to_phys(const_cast<Intel::Io_mmu::Irte*>(irt));
  for (auto &i: Intel::Io_mmu::iommus)
    i.set_irq_remapping_table(irt, irt_pa, IRT_size);

  int n_apics;
  for (n_apics = 0;
       auto ioapic = madt->find<Acpi_madt::Io_apic>(n_apics);
       ++n_apics)
    {
      Intel::Io_mmu *mmu = 0;
      ACPI::Dmar_dev_scope const *dev_scope = 0;

      for (auto &iommu: Intel::Io_mmu::iommus)
        {
          // support for multi-segment PCI missing
          if (iommu.segment != 0)
            continue;

          for (auto const &dev: iommu.devs)
            {
              // skip anything other than an IO-APIC
              if (dev.type != 0x3)
                continue;

              if (dev.enum_id == ioapic->id)
                {
                  mmu = &iommu;
                  dev_scope = &dev;
                  break;
                }
            }

          if (dev_scope)
            break;
        }

      if (!dev_scope)
        {
          WARN("IOMMU: Error, did not find DRHD entry for IO-APIC\n");
          continue;
        }

      if (dev_scope->end() - dev_scope->begin() != 1)
        WARN("IOMMU: Error, IO-APIC behind PCI bridges do not work\n");

      Unsigned16 srcid = (Unsigned16)dev_scope->start_bus_nr << 8;
      srcid |= ((Unsigned16)dev_scope->path[0].dev << 3) & 0xf8;
      srcid |= dev_scope->path[0].func & 0x7;

      Io_apic_remapped *apic;
      apic = new Boot_object<Io_apic_remapped>(ioapic->adr, ioapic->irq_base, mmu, srcid);
      (void)apic;

      if (Print_infos)
        apic->dump();
    }

  if (!n_apics)
    {
      WARN("IO-APIC: Could not find IO-APIC in MADT, skip init\n");
      return false;
    }

  if (Print_infos)
    printf("IO-APIC: dual 8259: %s\n", madt->apic_flags & 1 ? "yes" : "no");

  Irq_mgr_rmsi *m;
  Irq_mgr::mgr = m = new Boot_object<Irq_mgr_rmsi>(Intel::Io_mmu::iommus[0].irq_remapping_table());

  return true;
}

IMPLEMENTATION [debug]:

PUBLIC inline
char const *
Irq_chip_rmsi::chip_type() const override
{ return "rMSI"; }

PUBLIC inline
char const *
Io_apic_remapped::chip_type() const override
{ return "rIO-APIC"; }

