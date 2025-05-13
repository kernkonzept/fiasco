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
  explicit Irq_chip_rmsi(Intel::Io_mmu::Irte volatile *irt, bool coherent)
  : Irq_chip_ia32(Max_msis), _irt(irt), _coherent(coherent)
  {}

  unsigned nr_pins() const override { return Irq_chip_ia32::nr_pins(); }
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }

  Intel::Io_mmu::Irte volatile *_irt;
  /// Whether IOMMUs access to the IRT is cache coherent.
  bool _coherent;
};

class Irq_mgr_rmsi : public Io_apic_mgr
{
public:
  mutable Irq_chip_rmsi _chip;
};

enum { Print_info = 0 };

PRIVATE
void
Irq_chip_rmsi::clean_dcache(Intel::Io_mmu::Irte volatile const *irte) const
{
  if (!_coherent)
    Mem_unit::clean_dcache(const_cast<Intel::Io_mmu::Irte const *>(irte),
                           const_cast<Intel::Io_mmu::Irte const *>(irte) + 1);
}

PRIVATE inline NOEXPORT
void
Irq_chip_rmsi::inv_iec(unsigned vect)
{
  // All IOMMUs use the same interrupt remapping table.
  for (auto &iommu: Intel::Io_mmu::iommus)
    iommu.invalidate(Intel::Io_mmu::Inv_desc::iec(vect));
}

PRIVATE inline NOEXPORT
void
Irq_chip_rmsi::inv_iec_and_wait(unsigned vect)
{
  // All IOMMUs use the same interrupt remapping table.
  Intel::Io_mmu::queue_and_wait_on_all_iommus(Intel::Io_mmu::Inv_desc::iec(vect));
}

PUBLIC
bool
Irq_chip_rmsi::attach(Irq_base *irq, Mword pin, bool init = true) override
{ return valloc<Irq_chip_rmsi>(irq, pin, 0, init); }

PUBLIC
void
Irq_chip_rmsi::detach(Irq_base *irq) override
{
  extern char entry_int_apic_ignore[];
  unsigned vect = vector(irq->pin());

  _irt[vect].clear();
  clean_dcache(&_irt[vect]);
  inv_iec_and_wait(vect);

  vfree(irq, &entry_int_apic_ignore);
  Irq_chip_icu::detach(irq);
}

PUBLIC
int
Irq_chip_rmsi::msi_info(Mword pin, Unsigned64 src, Irq_mgr::Msi_info *info)
{
  if (pin >= _pins)
    return -L4_err::ERange;

  unsigned vect = vector(pin);
  if (!vect)
    return 0;

  Intel::Io_mmu::Irte entry = _irt[vect];
  bool need_flush = entry.present();

  entry.fpd() = 1;
  entry.vector() = vect;
  entry.src_info() = src;
  // To avoid having to wait for the invalidation of the interrupt remapping
  // table entry to complete whenever the target CPU of an IRQ is changed (in
  // set_cpu), directly assign a valid destination CPU ID here.
  entry.set_dst(::Apic::apic.cpu(Cpu_number::boot_cpu())->cpu_id());

  _irt[vect] = entry;
  clean_dcache(&_irt[vect]);

  if (need_flush)
    inv_iec_and_wait(vect);

  info->data = 0;
  info->addr = 0xfee00010 | (vect << 5);

  return 0;
}

PUBLIC
int
Irq_chip_rmsi::set_mode(Mword, Mode) override
{ return 0; }

PUBLIC
bool
Irq_chip_rmsi::is_edge_triggered(Mword) const override
{ return true; }

PUBLIC
bool
Irq_chip_rmsi::set_cpu(Mword pin, Cpu_number cpu) override
{
  unsigned vect = vector(pin);
  if (!vect)
    return false;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte entry = irte;

  Cpu_phys_id target = Apic::apic.cpu(cpu)->cpu_id();
  if (target == entry.get_dst())
    return false;

  entry.set_dst(target);

  irte = entry;
  clean_dcache(&irte);

  if (entry.present())
    // There is no need to wait for the IRTE invalidation to complete. If an IRQ
    // occurs while the old CPU is still targeted by an IRT cache entry that has
    // not yet been invalidated, the old CPU will forward the IRQ to the new
    // CPU. The case that a cache entry points to a CPU that went offline,
    // resulting in IRQs being lost, is not relevant because we always assign a
    // valid target CPU to the entry in msi_info() and Fiasco does not yet
    // implement a mechanism to move IRQs to another online CPU before taking
    // a CPU offline.
    inv_iec(vect);

  return true;
}

PUBLIC
void
Irq_chip_rmsi::mask(Mword pin) override
{
  unsigned vect = vector(pin);
  if (!vect)
    return;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte entry = irte;
  if (!entry.present())
    return;

  entry.present() = 0;
  irte = entry;
  clean_dcache(&irte);
  // There is no need to wait for the IRTE invalidation to complete, because
  // masking MSIs (edge triggered) is anyway only used by the kernel to protect
  // itself against devices triggering excessive numbers of interrupts. So it
  // doesn't really matter if an MSI still hits after returning from mask, due
  // to a not yet completed IRTE invalidation.
  inv_iec(vect);
}

PUBLIC
void
Irq_chip_rmsi::ack(Mword) override
{ ::Apic::irq_ack(); }

PUBLIC
void
Irq_chip_rmsi::mask_and_ack(Mword pin) override
{
  Irq_chip_rmsi::mask(pin);
  Irq_chip_rmsi::ack(pin);
}

PUBLIC
void
Irq_chip_rmsi::unmask(Mword pin) override
{
  unsigned vect = vector(pin);
  if (!vect)
    return;

  Intel::Io_mmu::Irte volatile &irte = _irt[vect];
  Intel::Io_mmu::Irte entry = irte;
  // ignore half initialized MSIs, need to use before unmask
  if (entry.vector() != vect)
    return;

  if (entry.present())
    return;

  entry.present() = 1;
  irte = entry;
  clean_dcache(&irte);
}

PUBLIC inline explicit
Irq_mgr_rmsi::Irq_mgr_rmsi(Intel::Io_mmu::Irte volatile *irt, bool coherent)
: _chip(irt, coherent)
{}

PUBLIC
Irq_mgr::Chip_pin
Irq_mgr_rmsi::chip_pin(Mword gsi) const override
{
  if (gsi & Msi_bit)
    return Chip_pin(&_chip, gsi & ~Msi_bit);

  return Io_apic_mgr::chip_pin(gsi);
}

PUBLIC
unsigned
Irq_mgr_rmsi::nr_gsis() const override
{ return Io_apic_mgr::nr_gsis(); }

PUBLIC
unsigned
Irq_mgr_rmsi::nr_msis() const override
{ return _chip.nr_pins(); }

PUBLIC
int
Irq_mgr_rmsi::msi_info(Mword msi, Unsigned64 src, Msi_info *info) const override
{
  if (msi & Msi_bit)
    return _chip.msi_info(msi & ~Msi_bit, src, info);

  return -L4_err::ERange;
}

PUBLIC
Mword
Irq_mgr_rmsi::legacy_override(Mword isa_pin) override
{
  if (isa_pin & Msi_bit)
    return isa_pin;

  return Io_apic_mgr::legacy_override(isa_pin);
}

PUBLIC
bool
Io_apic_remapped::attach(Irq_base *irq, Mword pin, bool init = true) override
{
  unsigned v = valloc<Io_apic_remapped>(irq, pin, 0, init);

  if (!v)
    return false;

  Io_apic_entry entry = read_entry(pin);
  Intel::Io_mmu::Irte irte;

  irte.present() = 1;
  irte.fpd() = 1;
  irte.vector() = v;
  irte.tm() = entry.trigger();
  irte.svt() = 1;
  irte.sid() = _src_id;
  // To avoid having to wait for the invalidation of the interrupt remapping
  // table entry to complete whenever the target CPU of an IRQ is changed (in
  // set_cpu), directly assign a valid destination CPU ID here.
  irte.set_dst(::Apic::apic.cpu(Cpu_number::boot_cpu())->cpu_id());

  _iommu->set_irq_mapping(irte, v, Intel::Io_mmu::Flush_op::No_flush);

  entry.format() = 1;
  entry.vector() = v;
  entry.irt_index() = v;

  write_entry(pin, entry);
  return true;
}

PUBLIC
void
Io_apic_remapped::detach(Irq_base *irq) override
{
  unsigned vect = vector(irq->pin());
  _iommu->set_irq_mapping(Intel::Io_mmu::Irte(), vect,
                          Intel::Io_mmu::Flush_op::Flush_and_wait);
  Io_apic::detach(irq);
}

PUBLIC
int
Io_apic_remapped::set_mode(Mword pin, Mode mode) override
{
  if (!mode.set_mode())
    return 0;

  Io_apic::set_mode(pin, mode);

  unsigned vect = vector(pin);
  if (EXPECT_FALSE(vect == 0))
    return 0;

  Intel::Io_mmu::Irte irte = _iommu->get_irq_mapping(vect);
  bool tm = to_io_apic_trigger(mode);
  if (tm != irte.tm())
    {
      irte.tm() = tm;
      _iommu->set_irq_mapping(irte, vect,
                              Intel::Io_mmu::Flush_op::Flush_and_wait);
    }

  return 0;
}

PUBLIC
bool
Io_apic_remapped::set_cpu(Mword pin, Cpu_number cpu) override
{
  unsigned vect = vector(pin);
  if (!vect)
    return false;

  Intel::Io_mmu::Irte entry = _iommu->get_irq_mapping(vect);
  Cpu_phys_id target = ::Apic::apic.cpu(cpu)->cpu_id();

  if (entry.get_dst() == target)
    return false;

  entry.set_dst(target);

  // There is no need to wait for the IRTE invalidation to complete. If an IRQ
  // occurs while the old CPU is still targeted by an IRT cache entry that has
  // not yet been invalidated, the old CPU will forward the IRQ to the new CPU.
  // The case that a cache entry points to a CPU that went offline, resulting in
  // IRQs being lost, is not relevant because we always assign a valid target
  // CPU to the entry in attach() and Fiasco does not yet implement a mechanism
  // to move IRQs to another online CPU before taking a CPU offline.
  _iommu->set_irq_mapping(entry, vect, Intel::Io_mmu::Flush_op::Flush);

  return true;
}

PUBLIC static FIASCO_INIT
bool
Io_apic_remapped::init_apics()
{
  auto madt = lookup_madt();

  // No MADT -> no I/O APIC -> no IRQ remapping.
  if (!madt)
    return false;

  // No DMAR or no INTR_REMAP supported -> fall back to normal I/O APIC.
  if (!Intel::Io_mmu::dmar_flags.intr_remap())
    return Io_apic::init_scan_apics();

  enum
  {
    IRT_size = 8,
    Num_irtes = 1 << IRT_size
  };

  typedef Intel::Io_mmu::Irte Irte;

  Irte *irt = new (Kmem_alloc::allocator()->alloc(Bytes(Num_irtes * sizeof(Irte))))
                  Irte[Num_irtes];
  if (!irt)
    panic("IOMMU: Could not allocate interrupt remapping table\n");

  bool use_x2apic = ::Apic::use_x2apic();
  bool coherent = true;
  Address irt_pa = Kmem::virt_to_phys(irt);

  for (auto &iommu: Intel::Io_mmu::iommus)
    {
      iommu.set_irq_remapping_table(irt, irt_pa, IRT_size, use_x2apic);
      coherent &= iommu.coherent();
    }

  unsigned n_apics = 0;
  for (auto *ioapic : madt->iterate<Acpi_madt::Io_apic>())
    {
      Intel::Io_mmu *mmu = nullptr;
      ACPI::Dmar_dev_scope const *dev_scope = nullptr;

      for (auto &iommu: Intel::Io_mmu::iommus)
        {
          // Support for multi-segment PCI missing.
          if (iommu.segment != 0)
            continue;

          for (auto const &dev: iommu.devs)
            {
              // Skip anything other than an I/O APIC.
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
          WARN("IOMMU: Did not find DRHD entry for I/O APIC\n");
          continue;
        }

      if (dev_scope->end() - dev_scope->begin() != 1)
        WARN("IOMMU: I/O APIC behind PCI bridges do not work\n");

      Unsigned16 srcid = Unsigned16{dev_scope->start_bus_nr} << 8;
      srcid |= (Unsigned16{dev_scope->path[0].dev} << 3) & 0xf8;
      srcid |= dev_scope->path[0].func & 0x7;

      Io_apic_remapped *apic =
        new Boot_object<Io_apic_remapped>(ioapic->addr, ioapic->irq_base, mmu,
                                          srcid);
      ++n_apics;

      if constexpr (Print_info)
        apic->dump();
    }

  if (!n_apics)
    {
      WARN("I/O APIC: Could not find I/O APIC in MADT, skipping initialization\n");
      return false;
    }

  if constexpr (Print_info)
    printf("I/O APIC: Dual 8259 %s\n",
           madt->apic_flags & 1 ? "present" : "absent");

  Irq_mgr_rmsi *m;
  Irq_mgr::mgr = m = new Boot_object<Irq_mgr_rmsi>(irt, coherent);

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
