INTERFACE:

#include <types.h>
#include "initcalls.h"
#include <spin_lock.h>
#include "irq_chip_ia32.h"
#include <cxx/bitfield>
#include "irq_mgr.h"
#include "pm.h"

class Acpi_madt;

class Io_apic_entry
{
  friend class Io_apic;
private:
  Unsigned64 _e;

public:
  enum Delivery { Fixed, Lowest_prio, SMI, NMI = 4, INIT, ExtINT = 7 };
  enum Dest_mode { Physical, Logical };
  enum Polarity { High_active, Low_active };
  enum Trigger { Edge, Level };

  Io_apic_entry() {}
  Io_apic_entry(Unsigned8 vector, Delivery d, Dest_mode dm, Polarity p,
                Trigger t, Unsigned32 dest)
    : _e(  vector_bfm_t::val(vector) | delivery_bfm_t::val(d) | mask_bfm_t::val(1)
         | dest_mode_bfm_t::val(dm)  | polarity_bfm_t::val(p)
         | trigger_bfm_t::val(t)     | dest_bfm_t::val(dest >> 24))
  {}

  CXX_BITFIELD_MEMBER( 0,  7, vector, _e);
  CXX_BITFIELD_MEMBER( 8, 10, delivery, _e);
  CXX_BITFIELD_MEMBER(11, 11, dest_mode, _e);
  CXX_BITFIELD_MEMBER(13, 13, polarity, _e);
  CXX_BITFIELD_MEMBER(15, 15, trigger, _e);
  CXX_BITFIELD_MEMBER(16, 16, mask, _e);
  // support for IRQ remapping
  CXX_BITFIELD_MEMBER(48, 48, format, _e);
  // support for IRQ remapping
  CXX_BITFIELD_MEMBER(49, 63, irt_index, _e);
  CXX_BITFIELD_MEMBER(56, 63, dest, _e);
};


class Io_apic : public Irq_chip_icu, protected Irq_chip_ia32
{
  friend class Jdb_io_apic_module;
  friend class Irq_chip_ia32;
public:
  unsigned nr_irqs() const override { return Irq_chip_ia32::nr_irqs(); }
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }

private:
  struct Apic
  {
    Unsigned32 volatile adr;
    Unsigned32 dummy[3];
    Unsigned32 volatile data;

    unsigned num_entries();
    Mword read(int reg);
    void modify(int reg, Mword set_bits, Mword del_bits);
    void write(int reg, Mword value);
  } __attribute__((packed));

  Apic *_apic;
  mutable Spin_lock<> _l;
  unsigned _offset;
  Io_apic *_next;

  static unsigned _nr_irqs;
  static Io_apic *_first;
  static Acpi_madt const *_madt;
  static Io_apic_entry *_state_save_area;
};

class Io_apic_mgr : public Irq_mgr, public Pm_object
{
public:
  Io_apic_mgr() { register_pm(Cpu_number::boot_cpu()); }
};


IMPLEMENTATION:

#include "acpi.h"
#include "apic.h"
#include "assert.h"
#include "kmem.h"
#include "kip.h"
#include "lock_guard.h"
#include "boot_alloc.h"
#include "warn.h"

enum { Print_info = 0 };

Acpi_madt const *Io_apic::_madt;
unsigned Io_apic::_nr_irqs;
Io_apic *Io_apic::_first;
Io_apic_entry *Io_apic::_state_save_area;


PUBLIC Irq_mgr::Irq
Io_apic_mgr::chip(Mword irq) const
{
  Io_apic *a = Io_apic::find_apic(irq);
  if (a)
    return Irq(a, irq - a->gsi_offset());

  return Irq(0, 0);
}

PUBLIC
unsigned
Io_apic_mgr::nr_irqs() const
{
  return Io_apic::total_irqs();
}

PUBLIC
unsigned
Io_apic_mgr::nr_msis() const
{ return 0; }

PUBLIC unsigned
Io_apic_mgr::legacy_override(Mword i)
{ return Io_apic::legacy_override(i); }

PUBLIC void
Io_apic_mgr::pm_on_suspend(Cpu_number cpu)
{
  (void)cpu;
  assert (cpu == Cpu_number::boot_cpu());
  Io_apic::save_state();
}

PUBLIC void
Io_apic_mgr::pm_on_resume(Cpu_number cpu)
{
  (void)cpu;
  assert (cpu == Cpu_number::boot_cpu());
  Pic::disable_all_save();
  Io_apic::restore_state(true);
}


IMPLEMENT inline
Mword
Io_apic::Apic::read(int reg)
{
  adr = reg;
  asm volatile ("": : :"memory");
  return data;
}

IMPLEMENT inline
void
Io_apic::Apic::modify(int reg, Mword set_bits, Mword del_bits)
{
  Mword tmp;
  adr = reg;
  asm volatile ("": : :"memory");
  tmp = data;
  tmp &= ~del_bits;
  tmp |= set_bits;
  data = tmp;
}

IMPLEMENT inline
void
Io_apic::Apic::write(int reg, Mword value)
{
  adr = reg;
  asm volatile ("": : :"memory");
  data = value;
}

IMPLEMENT inline
unsigned
Io_apic::Apic::num_entries()
{
  return (read(1) >> 16) & 0xff;
}

PUBLIC explicit
Io_apic::Io_apic(Unsigned64 phys, unsigned gsi_base)
: Irq_chip_ia32(0), _l(Spin_lock<>::Unlocked),
  _offset(gsi_base), _next(0)
{
  if (Print_info)
    printf("IO-APIC: addr=%lx\n", (Mword)phys);

  Address offs;
  Address va = Mem_layout::alloc_io_vmem(Config::PAGE_SIZE);
  assert (va);

  Kmem::map_phys_page(phys, va, false, true, &offs);

  Kip::k()->add_mem_region(Mem_desc(phys, phys + Config::PAGE_SIZE -1, Mem_desc::Reserved));

  Io_apic::Apic *a = (Io_apic::Apic*)(va + offs);
  a->write(0, 0);

  _apic = a;
  _irqs = a->num_entries() + 1;
  _entry = new Irq_entry_code[_irqs];

  if ((_offset + nr_irqs()) > _nr_irqs)
    _nr_irqs = _offset + nr_irqs();

  Io_apic **c = &_first;
  while (*c && (*c)->_offset < _offset)
    c = &((*c)->_next);

  _next = *c;
  *c = this;

  Mword cpu_phys = ::Apic::apic.cpu(Cpu_number::boot_cpu())->apic_id();

  for (unsigned i = 0; i < _irqs; ++i)
    {
      int v = 0x20 + i;
      Io_apic_entry e(v, Io_apic_entry::Fixed, Io_apic_entry::Physical,
                      Io_apic_entry::High_active, Io_apic_entry::Edge,
                      cpu_phys);
      write_entry(i, e);
    }
}


PUBLIC inline NEEDS["assert.h", "lock_guard.h"]
Io_apic_entry
Io_apic::read_entry(unsigned i) const
{
  auto g = lock_guard(_l);
  Io_apic_entry e;
  //assert(i <= num_entries());
  e._e = (Unsigned64)_apic->read(0x10+2*i) | (((Unsigned64)_apic->read(0x11+2*i)) << 32);
  return e;
}


PUBLIC inline NEEDS["assert.h", "lock_guard.h"]
void
Io_apic::write_entry(unsigned i, Io_apic_entry const &e)
{
  auto g = lock_guard(_l);
  //assert(i <= num_entries());
  _apic->write(0x10+2*i, e._e);
  _apic->write(0x11+2*i, e._e >> 32);
}

PROTECTED static FIASCO_INIT
void
Io_apic::read_overrides()
{
  for (unsigned tmp = 0;; ++tmp)
    {
      auto irq = _madt->find<Acpi_madt::Irq_source>(tmp);

      if (!irq)
        break;

      if (Print_info)
        printf("IO-APIC: ovr[%2u] %02x -> %x %x\n",
               tmp, (unsigned)irq->src, irq->irq, (unsigned)irq->flags);

      if (irq->irq >= _nr_irqs)
        {
          WARN("IO-APIC: warning override %02x -> %x (flags=%x) points to invalid GSI\n",
               (unsigned)irq->src, irq->irq, (unsigned)irq->flags);
          continue;
        }

      Io_apic *ioapic = find_apic(irq->irq);
      assert (ioapic);
      unsigned pin = irq->irq - ioapic->gsi_offset();
      Irq_chip::Mode mode = ioapic->get_mode(pin);

      unsigned pol = irq->flags & 0x3;
      unsigned trg = (irq->flags >> 2) & 0x3;
      switch (pol)
        {
        default: break;
        case 0: break;
        case 1: mode.polarity() = Mode::Polarity_high; break;
        case 2: break;
        case 3: mode.polarity() = Mode::Polarity_low; break;
        }

      switch (trg)
        {
        default: break;
        case 0: break;
        case 1: mode.level_triggered() = Mode::Trigger_edge; break;
        case 2: break;
        case 3: mode.level_triggered() = Mode::Trigger_level; break;
        }

      ioapic->set_mode(pin, mode);
    }
}


PUBLIC static FIASCO_INIT
Acpi_madt const *
Io_apic::lookup_madt()
{
  if (_madt)
    return _madt;

  _madt = Acpi::find<Acpi_madt const *>("APIC");
  return _madt;
}

PUBLIC static inline
Acpi_madt const *Io_apic::madt() { return _madt; }

PUBLIC static FIASCO_INIT
bool
Io_apic::init_scan_apics()
{
  auto madt = lookup_madt();

  if (madt == 0)
    {
      WARN("Could not find APIC in RSDT nor XSDT, skipping init\n");
      return false;
    }

  int n_apics;
  for (n_apics = 0;
       auto ioapic = madt->find<Acpi_madt::Io_apic>(n_apics);
       ++n_apics)
    {
      Io_apic *apic = new Boot_object<Io_apic>(ioapic->adr, ioapic->irq_base);
      (void)apic;

      if (Print_info)
        {
          printf("IO-APIC[%2d]: pins %u\n", n_apics, apic->nr_irqs());
          apic->dump();
        }
    }

  if (!n_apics)
    {
      WARN("IO-APIC: Could not find IO-APIC in MADT, skip init\n");
      return false;
    }

  if (Print_info)
    printf("IO-APIC: dual 8259: %s\n", madt->apic_flags & 1 ? "yes" : "no");

  return true;
}


PUBLIC static FIASCO_INIT
void
Io_apic::init(Cpu_number)
{
  if (!Irq_mgr::mgr)
    Irq_mgr::mgr = new Boot_object<Io_apic_mgr>();

  _state_save_area = new Boot_object<Io_apic_entry>[_nr_irqs];
  read_overrides();

  // in the case we use the IO-APIC not the PIC we can dynamically use
  // INT vectors from 0x20 to 0x2f too
  _vectors.add_free(0x20, 0x30);
};

PUBLIC static
void
Io_apic::save_state()
{
  for (Io_apic *a = _first; a; a = a->_next)
    for (unsigned i = 0; i < a->_irqs; ++i)
      _state_save_area[a->_offset + i] = a->read_entry(i);
}

PUBLIC static
void
Io_apic::restore_state(bool set_boot_cpu = false)
{
  Mword cpu_phys = 0;
  if (set_boot_cpu)
    cpu_phys = ::Apic::apic.cpu(Cpu_number::boot_cpu())->apic_id();

  for (Io_apic *a = _first; a; a = a->_next)
    for (unsigned i = 0; i < a->_irqs; ++i)
      {
        Io_apic_entry e = _state_save_area[a->_offset + i];
        if (set_boot_cpu && e.format() == 0)
          e.dest() = cpu_phys;
        a->write_entry(i, e);
      }
}

PUBLIC static
unsigned
Io_apic::total_irqs()
{ return _nr_irqs; }

PUBLIC static
unsigned
Io_apic::legacy_override(unsigned i)
{
  if (!_madt)
    return i;

  unsigned tmp = 0;
  for (;;++tmp)
    {
      Acpi_madt::Irq_source const *irq
	= static_cast<Acpi_madt::Irq_source const *>(_madt->find(Acpi_madt::Irq_src_ovr, tmp));

      if (!irq)
	break;

      if (irq->src == i)
	return irq->irq;
    }
  return i;
}

PUBLIC
void
Io_apic::dump()
{
  for (unsigned i = 0; i < _irqs; ++i)
    {
      Io_apic_entry e = read_entry(i);
      printf("  PIN[%2u%c]: vector=%2x, del=%u, dm=%s, dest=%u (%s, %s)\n",
	     i, e.mask() ? 'm' : '.',
	     (unsigned)e.vector(), (unsigned)e.delivery(), e.dest_mode() ? "logical" : "physical",
	     (unsigned)e.dest(),
	     e.polarity() ? "low" : "high",
	     e.trigger() ? "level" : "edge");
    }

}

PUBLIC inline
bool
Io_apic::valid() const { return _apic; }

PRIVATE inline NEEDS["assert.h", "lock_guard.h"]
void
Io_apic::_mask(unsigned irq)
{
  auto g = lock_guard(_l);
  //assert(irq <= _apic->num_entries());
  _apic->modify(0x10 + irq * 2, 1UL << 16, 0);
}

PRIVATE inline NEEDS["assert.h", "lock_guard.h"]
void
Io_apic::_unmask(unsigned irq)
{
  auto g = lock_guard(_l);
  //assert(irq <= _apic->num_entries());
  _apic->modify(0x10 + irq * 2, 0, 1UL << 16);
}

PUBLIC inline NEEDS["assert.h", "lock_guard.h"]
bool
Io_apic::masked(unsigned irq)
{
  auto g = lock_guard(_l);
  //assert(irq <= _apic->num_entries());
  return _apic->read(0x10 + irq * 2) & (1UL << 16);
}

PUBLIC inline
void
Io_apic::sync()
{
  (void)_apic->data;
}

PUBLIC inline NEEDS["assert.h", "lock_guard.h"]
void
Io_apic::set_dest(unsigned irq, Mword dst)
{
  auto g = lock_guard(_l);
  //assert(irq <= _apic->num_entries());
  _apic->modify(0x11 + irq * 2, dst & (~0UL << 24), ~0UL << 24);
}

PUBLIC inline
unsigned
Io_apic::gsi_offset() const { return _offset; }

PUBLIC static
Io_apic *
Io_apic::find_apic(unsigned irqnum)
{
  for (Io_apic *a = _first; a; a = a->_next)
    {
      if (a->_offset <= irqnum && a->_offset + a->_irqs > irqnum)
	return a;
    }
  return 0;
};

PUBLIC void
Io_apic::mask(Mword irq) override
{
  _mask(irq);
  sync();
}

PUBLIC void
Io_apic::ack(Mword) override
{
  ::Apic::irq_ack();
}

PUBLIC void
Io_apic::mask_and_ack(Mword irq) override
{
  _mask(irq);
  sync();
  ::Apic::irq_ack();
}

PUBLIC void
Io_apic::unmask(Mword irq) override
{
  _unmask(irq);
}

PUBLIC void
Io_apic::set_cpu(Mword irq, Cpu_number cpu) override
{
  set_dest(irq, ::Apic::apic.cpu(cpu)->apic_id());
}

PROTECTED static inline
Mword
Io_apic::to_io_apic_trigger(Irq_chip::Mode mode)
{
  return mode.level_triggered()
         ? Io_apic_entry::Level
         : Io_apic_entry::Edge;
}

PROTECTED static inline
Mword
Io_apic::to_io_apic_polarity(Irq_chip::Mode mode)
{
  return mode.polarity() == Irq_chip::Mode::Polarity_high
         ? Io_apic_entry::High_active
         : Io_apic_entry::Low_active;
}

PUBLIC int
Io_apic::set_mode(Mword pin, Mode mode) override
{
  if (!mode.set_mode())
    return 0;

  Io_apic_entry e = read_entry(pin);
  e.polarity() = to_io_apic_polarity(mode);
  e.trigger() = to_io_apic_trigger(mode);
  write_entry(pin, e);
  return 0;
}

PUBLIC inline
Irq_chip::Mode
Io_apic::get_mode(Mword pin)
{
  Io_apic_entry e = read_entry(pin);
  Mode m(Mode::Set_irq_mode);
  m.polarity() = e.polarity() == Io_apic_entry::High_active
               ? Mode::Polarity_high
               : Mode::Polarity_low;
  m.level_triggered() = e.trigger() == Io_apic_entry::Level
                      ? Mode::Trigger_level
                      : Mode::Trigger_edge;
  return m;
}

PUBLIC
bool
Io_apic::is_edge_triggered(Mword pin) const override
{
  Io_apic_entry e = read_entry(pin);
  return e.trigger() == Io_apic_entry::Edge;
}

PUBLIC
bool
Io_apic::alloc(Irq_base *irq, Mword pin) override
{
  unsigned v = valloc<Io_apic>(irq, pin, 0);

  if (!v)
    return false;

  Io_apic_entry e = read_entry(pin);
  e.vector() = v;
  write_entry(pin, e);
  return true;
}

PUBLIC
void
Io_apic::unbind(Irq_base *irq) override
{
  extern char entry_int_apic_ignore[];
  Mword n = irq->pin();
  mask(n);
  vfree(irq, &entry_int_apic_ignore);
  Irq_chip_icu::unbind(irq);
}

PUBLIC static inline
bool
Io_apic::active()
{ return _first; }

IMPLEMENTATION [debug]:

PUBLIC inline
char const *
Io_apic::chip_type() const override
{ return "IO-APIC"; }

