INTERFACE:

#include "irq_chip.h"
#include "mem.h"
#include "mmio_register_block.h"

#include <cxx/bitfield>

class Gic_redist
{
private:
  Mmio_register_block _redist;

public:
  enum
  {
    GICR_CTRL         = 0x0000,
    GICR_IIDR         = 0x0004,
    GICR_TYPER        = 0x0008,
    GICR_STATUSR      = 0x0010,
    GICR_WAKER        = 0x0014,
    GICR_PROPBASER    = 0x0070,
    GICR_PENDBASER    = 0x0078,
    GICR_PIDR2        = 0xffe8,
    GICR_SGI_BASE     = 0x10000,
    GICR_IGROUPR0     = GICR_SGI_BASE + 0x0080,
    GICR_ISENABLER0   = GICR_SGI_BASE + 0x0100,
    GICR_ICENABLER0   = GICR_SGI_BASE + 0x0180,
    GICR_ISPENDR0     = GICR_SGI_BASE + 0x0200,
    GICR_ICPENDR0     = GICR_SGI_BASE + 0x0280,
    GICR_ISACTIVER0   = GICR_SGI_BASE + 0x0300,
    GICR_ICACTIVER0   = GICR_SGI_BASE + 0x0380,
    GICR_IPRIORITYR0  = GICR_SGI_BASE + 0x0400,
    GICR_ICFGR0       = GICR_SGI_BASE + 0x0c00,

    GICR_frame_size   = 0x10000,

    GICR_WAKER_Processor_sleep = 1 << 1,
    GICR_WAKER_Children_asleep = 1 << 2,
  };

  struct Typer
  {
    Unsigned64 raw;
    Typer() = default;
    explicit Typer(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_RO( 0,  0, plpis, raw);
    CXX_BITFIELD_MEMBER_RO( 1,  1, vlpis, raw);
    CXX_BITFIELD_MEMBER_RO( 4,  4, last, raw);
    CXX_BITFIELD_MEMBER_RO(32, 63, affinity, raw);
  };
};

// ------------------------------------------------------------------------
INTERFACE[arm_gic_msi]:

#include "gic_mem.h"

EXTENSION class Gic_redist
{
private:
  enum
  {
    GICR_lpi_default_prio = 0xa0,

    GICR_config_table_align  = 0x1000,
    GICR_pending_table_align = 0x10000,
  };

  struct Ctrl
  {
    Unsigned32 raw;
    Ctrl() = default;
    explicit Ctrl(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  0, enable_lpis, raw);
  };

  struct Propbaser
  {
    Unsigned64 raw;
    Propbaser() = default;
    explicit Propbaser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  4, id_bits, raw);
    CXX_BITFIELD_MEMBER          ( 7,  9, cacheability, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(12, 51, pa, raw);
  };

  struct Pendbaser
  {
    Unsigned64 raw;
    Pendbaser() = default;
    explicit Pendbaser(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 7,  9, cacheability, raw);
    CXX_BITFIELD_MEMBER          (10, 11, shareability, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(16, 51, pa, raw);
    CXX_BITFIELD_MEMBER          (62, 62, ptz, raw);
  };

  Gic_mem::Mem_chunk _lpi_pending_table;

  static unsigned num_lpi_intid_bits;
  static Gic_mem::Mem_chunk lpi_config_table;
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "cpu.h"
#include "l4_types.h"
#include "kmem_alloc.h"
#include "panic.h"
#include "poll_timeout_counter.h"
#include <cstdio>
#include <string.h>

PUBLIC
void
Gic_redist::find(Address base, Unsigned64 mpidr, Cpu_number cpu)
{
  unsigned o = 0;
  Typer gicr_typer;
  Unsigned64 typer_aff = (mpidr & 0x0000ffffff) | ((mpidr & 0xff00000000) >> 8);
  do
    {
      Mmio_register_block r(base + o);

      unsigned arch_rev = (r.read<Unsigned32>(GICR_PIDR2) >> 4) & 0xf;
      if (arch_rev != 0x3 && arch_rev != 0x4)
        // No GICv3 and no GICv4
        break;

      gicr_typer.raw = r.read<Unsigned64>(GICR_TYPER);
      if (gicr_typer.affinity() == typer_aff)
        {
          printf("CPU%d: GIC Redistributor at %lx for 0x%llx\n",
                 cxx::int_value<Cpu_number>(cpu),
                 r.get_mmio_base(), mpidr & ~0xc0000000ull);
          _redist = r;
          return;
        }

      o += 2 * GICR_frame_size;
      if (gicr_typer.vlpis())
        o += 2 * GICR_frame_size;
    }
  while (!gicr_typer.last());

  panic("GIC: Did not find a redistributor for CPU%d\n",
        cxx::int_value<Cpu_number>(cpu));
}

PUBLIC
void
Gic_redist::cpu_init()
{
  unsigned val = _redist.read<Unsigned32>(GICR_WAKER);
  if (val & GICR_WAKER_Children_asleep)
    {
      val &= ~GICR_WAKER_Processor_sleep;
      _redist.write<Unsigned32>(val, GICR_WAKER);

      L4::Poll_timeout_counter i(5000000);
      while (i.test(_redist.read<Unsigned32>(GICR_WAKER) & GICR_WAKER_Children_asleep))
        Proc::pause();

      if (i.timed_out())
        panic("GIC: redistributor did not awake\n");
    }

  _redist.write<Unsigned32>(0xffffffff, GICR_ICENABLER0);

  _redist.write<Unsigned32>(0x0000001e, GICR_ISENABLER0);
  _redist.write<Unsigned32>(0xffffffff, GICR_IGROUPR0);

  _redist.write<Unsigned32>(0xffffffff, GICR_ICPENDR0);
  _redist.write<Unsigned32>(0xffffffff, GICR_ICACTIVER0); // clear active

  for (unsigned g = 0; g < 32; g += 4)
    _redist.write<Unsigned32>(0xa0a0a0a0, GICR_IPRIORITYR0 + g);
}

PUBLIC
void
Gic_redist::mask(Mword pin)
{
  _redist.write<Unsigned32>(1u << pin, GICR_ICENABLER0);
}

PUBLIC
void
Gic_redist::unmask(Mword pin)
{
  _redist.write<Unsigned32>(1u << pin, GICR_ISENABLER0);
}

PUBLIC inline
void
Gic_redist::irq_prio(unsigned irq, unsigned prio)
{
  _redist.write<Unsigned8>(prio, GICR_IPRIORITYR0 + irq);
}

PUBLIC inline
unsigned
Gic_redist::irq_prio(unsigned irq)
{
  return _redist.read<Unsigned8>(GICR_IPRIORITYR0 + irq);
}

PUBLIC inline NEEDS["l4_types.h"]
int
Gic_redist::set_mode(Mword pin, Irq_chip::Mode m)
{
  if (!m.set_mode())
    return 0;

  unsigned v = 0;
  switch (m.flow_type())
    {
    case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high:
      break;
    case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high:
      v = 2;
      break;
    default:
      return -L4_err::EInval;
    };

  unsigned shift = (pin & 15) * 2;

  _redist.modify<Unsigned32>(v << shift, 3 << shift, GICR_ICFGR0 + (pin >> 4) * 4);

  return 0;
}

//-------------------------------------------------------------------
IMPLEMENTATION[arm_gic_msi]:

#include "gic.h"

#include <arithmetic.h>

unsigned Gic_redist::num_lpi_intid_bits = 0;
Gic_mem::Mem_chunk Gic_redist::lpi_config_table;

PUBLIC
static void
Gic_redist::init_lpi(unsigned num_lpis)
{
  num_lpi_intid_bits = cxx::log2u(Gic_dist::Lpi_intid_base + num_lpis - 1) + 1;
  num_lpis = (1U << num_lpi_intid_bits) - Gic_dist::Lpi_intid_base;

  lpi_config_table = Gic_mem::alloc_mem(num_lpis, GICR_config_table_align);
  if (!lpi_config_table.is_valid())
    panic("GIC: Failed to allocate redistributor LPI configuration table.\n");
  // Initialize all LPIs with default priority and disabled.
  memset(lpi_config_table.virt_ptr(), GICR_lpi_default_prio, num_lpis);
}

PUBLIC
void
Gic_redist::cpu_init_lpi()
{
  Typer gicr_typer(_redist.read<Unsigned64>(GICR_TYPER));
  if (!gicr_typer.plpis())
    panic("GIC: Redistributor does not support physical LPIs.\n");

  Ctrl ctrl(_redist.read<Unsigned32>(GICR_CTRL));
  if (ctrl.enable_lpis())
    panic("GIC: LPI support of redistributor is already enabled.\n");

  Propbaser propbaser;
  propbaser.id_bits() = num_lpi_intid_bits - 1;
  propbaser.pa() = lpi_config_table.phys_addr();
  lpi_config_table.setup_reg(_redist.r<Unsigned64>(GICR_PROPBASER), propbaser);
  lpi_config_table.make_coherent();

  // Each bit in the pending table represents the pending state of one LPI.
  // The first 1KB is reserved for the pending state of SGIs/PPIs/SPIs.
  unsigned lpi_pending_table_size = (1U << num_lpi_intid_bits) / 8;
  // Zero initialize pending table, no LPIs are pending.
  _lpi_pending_table = Gic_mem::alloc_zmem(lpi_pending_table_size,
                                           GICR_pending_table_align);
  if (!_lpi_pending_table.is_valid())
    panic("GIC: Failed to allocate redistributor LPI pending table.\n");

  Pendbaser pendbaser;
  pendbaser.pa() = _lpi_pending_table.phys_addr();
  pendbaser.ptz() = 1;
  _lpi_pending_table.setup_reg(_redist.r<Unsigned64>(GICR_PENDBASER), pendbaser);
  _lpi_pending_table.make_coherent();

  // Enable LPI support for redistributor.
  ctrl.enable_lpis() = 1;
  _redist.write<Unsigned32>(ctrl.raw, GICR_CTRL);
}

PUBLIC static inline
void
Gic_redist::enable_lpi(Mword lpi, bool enabled)
{
  Unsigned8 *lpi_config = lpi_config_table.virt_ptr<Unsigned8>() + lpi;
  write_now(lpi_config, GICR_lpi_default_prio | enabled);
  lpi_config_table.make_coherent(lpi_config, lpi_config + 1);
}

PUBLIC inline
Address
Gic_redist::get_base() const
{
  return _redist.get_mmio_base();
}
