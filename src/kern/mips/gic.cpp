INTERFACE:

#include "irq_chip_generic.h"
#include "mmio_register_block.h"

class Gic : public Irq_chip_gen
{
private:
  typedef Mword Reg_type;
  enum : unsigned long
  {
    Reg_bytes = sizeof(Reg_type),
    Reg_width = Reg_bytes * 8
  };

  enum : Address
  {
    Core_local = 0x8000,
    Core_other = Core_local + 0x4000,
    User_visible = 0x10000,

    Sh_config       = 0x0000,
    Sh_counter_lo   = 0x0010,
    Sh_counter_hi   = 0x0014,
    Sh_revision_id  = 0x0020,
    Sh_int_avail    = 0x0024, // .. incl. 0x40
    Sh_gid_config   = 0x0080, // .. incl. 0x9c
    Sh_pol          = 0x0100, // .. incl. 0x11c
    Sh_trigger      = 0x0180, // .. incl. 0x19c
    Sh_dual_edge    = 0x0200, // .. incl. 0x21c
    Sh_wedge        = 0x0280,
    Sh_rmask        = 0x0300, // .. incl. 0x31c
    Sh_smask        = 0x0380, // .. incl. 0x39c
    Sh_mask         = 0x0400, // .. incl. 0x41c
    Sh_pend         = 0x0480, // .. incl. 0x49c
    Sh_map_pin      = 0x0500, // .. incl. 0x8fc
    Sh_map_vpe      = 0x2000, // .. incl. 0x3fe4

    Core_ctl         = 0x0000,
    Core_pend        = 0x0004,
    Core_mask        = 0x0008,
    Core_rmask       = 0x000c,
    Core_smask       = 0x0010,
    Core_wd_map      = 0x0040,
    Core_compare_map = 0x0044,
    Core_timer_map   = 0x0048,
    Core_fdc_map     = 0x004c,
    Core_prefctr_map = 0x0050,
    Core_swint0_map  = 0x0054,
    Core_swint1_map  = 0x0058,
    Core_other_addr  = 0x0080,
    Core_ident       = 0x0088,
    Core_wd_config0  = 0x0090,
    Core_wd_count0   = 0x0094,
    Core_wd_initial0 = 0x0098,
    Core_compare_lo  = 0x00a0,
    Core_compare_hi  = 0x00a4,
    Core_coffset     = 0x0200,
    Core_dint_part   = 0x3000,
    Core_brk_part    = 0x3080,
  };

  enum {
    Cpu_int_offset = 2,
  };

  static Address sh_map_core(Address irq, Cpu_phys_id core)
  {
    return Sh_map_vpe + irq * 0x20UL
           + (cxx::int_value<Cpu_phys_id>(core) / Reg_width) * Reg_bytes;
  }

  static Reg_type sh_map_core_bit(Cpu_phys_id core)
  { return 1UL << (cxx::int_value<Cpu_phys_id>(core) % Reg_width); }

  static Address sh_map_pin(Address irq)
  { return Sh_map_pin + irq * 0x4UL; }

  static Address sh_irq_reg(Address reg, Address irq)
  { return reg + (irq / Reg_width) * Reg_bytes; }

  static Reg_type sh_irq_bit(Address irq)
  { return 1UL << (irq % Reg_width); }

  Register_block<Reg_width> _r;
  Spin_lock<> _mode_lock;

  Reg_type _enabled_map[256 / Reg_width];
};

// -----------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>
#include <cstdio>
#include <string.h>

PUBLIC
Gic::Gic(Address mmio, unsigned cpu_int) : _r(mmio), _mode_lock(Spin_lock<>::Unlocked)
{
  Reg_type cfg = _r[Sh_config];
  unsigned vpes = (cfg & 0x3f) + 1;
  unsigned nrirqs = (((cfg >> 16) & 0xff) + 1) * 8;
  Reg_type rev = _r[Sh_revision_id];

  printf("MIPS GIC[%08lx]: %u IRQs %u VPEs%s, V%d.%d\n",
         mmio, nrirqs, vpes, (cfg & (1 << 31)) ? "VZP" : "",
         (unsigned) rev >> 8, (unsigned) rev & 0xff);

  assert (vpes <= 32); // this limit is due to set_cpu limitations
  assert (nrirqs <= 256);

  assert (cpu_int >= Cpu_int_offset && cpu_int < 8);
  cpu_int -= Cpu_int_offset;

  init(nrirqs);

  memset(_enabled_map, 0, sizeof(_enabled_map));

  for (unsigned i = 0; i < (nrirqs + Reg_width - 1) / Reg_width; ++i)
    {
      _r[Sh_rmask   + i * Reg_bytes] = ~0UL; // disabled
      _r[Sh_pol     + i * Reg_bytes] = ~0UL; // pol high
      _r[Sh_trigger + i * Reg_bytes] = 0; // level
    }

  for (unsigned i = 0; i < nrirqs; ++i)
    {
      // default to core 0
      _r[sh_map_core(i, Cpu_phys_id(0))] = 1;
      for (unsigned id = Reg_width; id < 256; id += Reg_width)
        _r[sh_map_core(i, Cpu_phys_id(id))]  = 0;

      _r.r<Unsigned32>(sh_map_pin(i)) = (1UL << 31) | cpu_int;
    }

  setup_ipis();
}

PRIVATE inline
void
Gic::mod_mask(Mword pin, bool mask, unsigned hw_reg_base)
{
  auto b = sh_irq_bit(pin);

  _r[sh_irq_reg(hw_reg_base, pin)] = b;
  if (mask)
    _enabled_map[pin / Reg_width] &= ~b;
  else
    _enabled_map[pin / Reg_width] |= b;
}

PUBLIC
void
Gic::unmask(Mword pin) override
{
  mod_mask(pin, false, Sh_smask);
}

PUBLIC
void
Gic::mask(Mword pin) override
{
  mod_mask(pin, true, Sh_rmask);
}

PUBLIC
void
Gic::mask_and_ack(Mword pin) override
{
  mod_mask(pin, true, Sh_rmask);
}

PUBLIC
void
Gic::ack(Mword) override
{}

PUBLIC
int
Gic::set_mode(Mword pin, Mode mode) override
{
  if (!mode.set_mode())
    return 0;

  auto smask = sh_irq_reg(Sh_smask, pin);
  auto rmask = sh_irq_reg(Sh_rmask, pin);
  auto pol = sh_irq_reg(Sh_pol, pin);
  auto trig = sh_irq_reg(Sh_trigger, pin);
  auto dual = sh_irq_reg(Sh_dual_edge, pin);
  auto bit = sh_irq_bit(pin);

  auto guard = lock_guard(_mode_lock);

  bool lvl = mode.level_triggered();
  bool pol_h = mode.polarity() == Mode::Polarity_high;

  auto mask = _r.read<Reg_type>(sh_irq_reg(Sh_mask, pin)) & bit;
  if (mask)
    _r[rmask] = bit;

  _r[trig].modify(lvl ? 0 : bit, lvl ? bit : 0);
  _r[pol].modify(pol_h ? bit : 0, pol_h ? 0 : bit);
  if (!lvl)
    {
      bool d = mode.polarity() == Mode::Polarity_both;
      _r[dual].modify(d ? bit : 0, d ? 0 : bit);
    }

  if (mask)
    _r[smask] = bit;

  return 0;
}

PUBLIC
bool
Gic::is_edge_triggered(Mword pin) const override
{
  return sh_irq_bit(pin) & _r[sh_irq_reg(Sh_trigger, pin)];
}

PUBLIC
unsigned
Gic::pending()
{
  // We might also need to check that we're on the proper CPU but
  // lets postpone that until it is actually required
  for (unsigned i = 0, o = 0; i < nr_irqs(); o += Reg_bytes, i += Reg_width)
    if (_enabled_map[i / Reg_width])
      {
        Reg_type v = _r[Sh_pend + o] & _enabled_map[i / Reg_width];
        if (v)
          return i + __builtin_ffs(v) - 1;
      }

  return ~0U;
}

//---------------------------------------------------------------------------
INTERFACE [mp]:

#include "ipi_control.h"

EXTENSION class Gic : public Ipi_control
{
private:
  unsigned _ipi_base;
  unsigned _cpu_int_ipi = 6;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "cpu.h"
#include "ipi.h"
#include "mips_cpu_irqs.h"
#include "processor.h"

PUBLIC
void
Gic::set_cpu(Mword pin, Cpu_number cpu) override
{
  auto pcpu = Cpu::cpus.cpu(cpu).phys_id();
  // AW11: avoid setting two bits in two different words of the
  // GIC_SH_MAPi_COREn registers. So limit the maximum phys CPU number
  // to 31;
  assert (cxx::int_value<Cpu_phys_id>(pcpu) < 32);

  _r[sh_map_core(pin, pcpu)] = sh_map_core_bit(pcpu);
}

PUBLIC void
Gic::send_ipi(Cpu_number to, Ipi *) override
{
  unsigned irq = _ipi_base + cxx::int_value<Cpu_number>(to);
  _r[Sh_wedge] = (1UL << 31) | irq;
}

PUBLIC void
Gic::ack_ipi(Cpu_number cpu) override
{
  asm volatile ("sync" : : : "memory");
  unsigned irq = _ipi_base + cxx::int_value<Cpu_number>(cpu);
  _r[Sh_wedge] = irq;
}

PUBLIC void
Gic::init_ipis(Cpu_number cpu, Irq_base *irq) override
{
  if (cpu == Cpu_number::boot_cpu())
    check(Mips_cpu_irqs::chip->alloc(irq, _cpu_int_ipi));
  else
    Mips_cpu_irqs::chip->unmask(_cpu_int_ipi);

  unsigned i = _ipi_base + cxx::int_value<Cpu_number>(cpu);
  auto cpuid = Proc::cpu_id();
  _r[sh_map_core(i, cpuid)] = sh_map_core_bit(cpuid);
}

PRIVATE inline NOEXPORT
void
Gic::setup_ipis()
{
  /* make sure we have at least 16 (arbitrary) IRQs left after
   * assigning IPIs */
  assert (Config::Max_num_cpus <= (nr_irqs() - 16));
  _ipi_base = nr_irqs() - Config::Max_num_cpus;
  printf("GIC: IPI base is: %u\n", _ipi_base);

  //FIXME: limit the number of user visible IRQs by the IPIs allocated
  for (unsigned i = _ipi_base; i < nr_irqs(); ++i)
    {
      _r.r<Unsigned32>(sh_map_pin(i)) = (1UL << 31) | (_cpu_int_ipi - 2);
      auto mask = sh_irq_bit(i);
      _r[sh_irq_reg(Sh_pol, i)].set(mask);   // pol high
      _r[sh_irq_reg(Sh_trigger, i)].set(mask);   // edge
      _r[sh_irq_reg(Sh_dual_edge, i)].clear(mask); // single edge
      asm volatile ("sync" : : : "memory");
      _r[Sh_wedge] = i;                   // clear IRQ
      asm volatile ("sync" : : : "memory");
      _r[sh_irq_reg(Sh_smask, i)] = mask;      // enable
    }

  Ipi::hw = this;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PRIVATE inline NOEXPORT void Gic::setup_ipis() {}

PUBLIC inline
void
Gic::set_cpu(Mword, Cpu_number) override
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
char const *
Gic::chip_type() const override
{ return "GIC"; }
