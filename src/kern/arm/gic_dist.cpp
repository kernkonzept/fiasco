INTERFACE:

#include "irq_chip.h"
#include "mmio_register_block.h"
#include "spin_lock.h"

#include <cxx/bitfield>

class Gic_dist
{
private:
  Mmio_register_block _dist;
  Spin_lock<> _lock;

public:
  using V2 = cxx::integral_constant<int, 2>;
  using V3 = cxx::integral_constant<int, 3>;
  enum : unsigned
  {
    GICD_CTRL         = 0x000,
    GICD_TYPER        = 0x004,
    GICD_IIDR         = 0x008,
    GICD_IGROUPR      = 0x080,
    GICD_ISENABLER    = 0x100,
    GICD_ICENABLER    = 0x180,
    GICD_ISPENDR      = 0x200,
    GICD_ICPENDR      = 0x280,
    GICD_ISACTIVER    = 0x300,
    GICD_ICACTIVER    = 0x380,
    GICD_IPRIORITYR   = 0x400,
    GICD_ITARGETSR    = 0x800,
    GICD_ICFGR        = 0xc00,
    GICD_SGIR         = 0xf00,

    GICD_IROUTER      = 0x6000, // n = 0..31 reserved, n = 32...1019 valid

    // covers GICv2 and GICv3
    Size              = 0x10000,

    MXC_TZIC_PRIOMASK = 0x00c,
    MXC_TZIC_SYNCCTRL = 0x010,
    MXC_TZIC_PND      = 0xd00,

    GICD_CTRL_ENABLE         = 0x01,
    GICD_CTRL_ENGR1          = 0x01,
    GICD_CTRL_ENGR1A         = 0x02,
    GICD_CTRL_ARE_NS         = 0x10,
    GICD_CTRL_RWP            = 0x80000000,

    MXC_TZIC_CTRL_NSEN       = 1U << 16,
    MXC_TZIC_CTRL_NSENMASK   = 1U << 31,

    Lpi_intid_base = 8192,
  };

  struct Typer
  {
    Unsigned32 raw;
    explicit Typer(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_RO( 0,  4, num_spis, raw);
    CXX_BITFIELD_MEMBER_RO(10, 10, security_extn, raw);
  };

  struct Typer_v3 : public Typer
  {
    explicit Typer_v3(Unsigned32 v) : Typer(v) {}
    CXX_BITFIELD_MEMBER_RO( 5,  7, cpu_number, raw);
    CXX_BITFIELD_MEMBER_RO( 8,  8, espi, raw);
    CXX_BITFIELD_MEMBER_RO( 9,  9, nmi, raw);
    CXX_BITFIELD_MEMBER_RO(11, 15, num_lpis, raw);
    CXX_BITFIELD_MEMBER_RO(16, 16, mbis, raw);
    CXX_BITFIELD_MEMBER_RO(17, 17, lpis, raw);
    CXX_BITFIELD_MEMBER_RO(18, 18, dvis, raw);
    CXX_BITFIELD_MEMBER_RO(19, 23, id_bits, raw);
    CXX_BITFIELD_MEMBER_RO(24, 24, a3v, raw);
    CXX_BITFIELD_MEMBER_RO(25, 25, no1n, raw);
    CXX_BITFIELD_MEMBER_RO(26, 26, rss, raw);
    CXX_BITFIELD_MEMBER_RO(27, 31, espi_range, raw);
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pic_gic_mxc_tzic]:

#include <arithmetic.h>

EXTENSION class Gic_dist
{
public:
  static constexpr bool Config_mxc_tzic = true;

  Unsigned32 mxc_pending()
  {
    Address a = MXC_TZIC_PND;
    for (unsigned g = 0; g < 128; g += 32, a += 4)
      {
        Unsigned32 v = _dist.read<Unsigned32>(a);
        if (v)
          return g + cxx::log2u(v);
      }
    return 0;
  }
};

// ------------------------------------------------------------------------
INTERFACE [arm && !pic_gic_mxc_tzic]:

EXTENSION class Gic_dist
{
public:
  static constexpr bool Config_mxc_tzic = false;

  static constexpr Unsigned32 mxc_pending() { return 0; }
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_em_tz]:

EXTENSION class Gic_dist { static constexpr bool Config_tz_sec = true; };

// ------------------------------------------------------------------------
INTERFACE [arm && !arm_em_tz]:

EXTENSION class Gic_dist { static constexpr bool Config_tz_sec = false; };

//-------------------------------------------------------------------
IMPLEMENTATION [have_arm_gicv2]:

PUBLIC inline
void
Gic_dist::init_targets(unsigned max, V2)
{
  // use target from local IRQ 0-3
  Unsigned32 t = itarget(0);

  for (unsigned i = 32; i < max; i += 4)
    _dist.write<Unsigned32>(t, GICD_ITARGETSR + i);
}

PUBLIC inline
void
Gic_dist::set_cpu(Mword pin, Unsigned8 target, V2)
{
  // GICD_ITARGETSR<0..31> are read-only
  _dist.write<Unsigned8>(target, GICD_ITARGETSR + pin);
}

PUBLIC inline
Unsigned32
Gic_dist::itarget(unsigned offset)
{
  return _dist.read<Unsigned32>(GICD_ITARGETSR + offset);
}

PUBLIC inline
void
Gic_dist::igroup_init(V2, unsigned num)
{
  Mword v = 0;
  if (Config_tz_sec || Config_mxc_tzic)
    v = 0xffffffff;

  for (unsigned i = 32; i < num; i += 32)
    _dist.write<Unsigned32>(v, GICD_IGROUPR + i / 8);
}

PUBLIC inline
void
Gic_dist::enable(V2)
{
  Unsigned32 dist_enable = GICD_CTRL_ENABLE;
  if (Config_mxc_tzic && !Config_tz_sec)
    dist_enable |= MXC_TZIC_CTRL_NSEN | MXC_TZIC_CTRL_NSENMASK;

  _dist.write<Unsigned32>(dist_enable, GICD_CTRL);
}

PUBLIC inline
void
Gic_dist::cpu_init_v2()
{
  Mword sec_irqs;

  if (Config_tz_sec)
    sec_irqs = 0x00000f00;

  _dist.write<Unsigned32>(0xffffffff, GICD_ICENABLER);
  if (Config_tz_sec)
    {
      _dist.write<Unsigned32>(0x00000f00, GICD_ISENABLER);
      _dist.write<Unsigned32>(~sec_irqs, GICD_IGROUPR);
    }
  else
    {
      _dist.write<Unsigned32>(0x0000001e, GICD_ISENABLER);
      _dist.write<Unsigned32>(0, GICD_IGROUPR);
    }

  _dist.write<Unsigned32>(0xffffffff, GICD_ICPENDR);

  _dist.write<Unsigned32>(0xffffffff, GICD_ICACTIVER); // clear active
  _dist.write<Unsigned32>(0xffffffff, 0xf10); // sgi pending clear
  _dist.write<Unsigned32>(0xffffffff, 0xf14); // sgi pending clear
  _dist.write<Unsigned32>(0xffffffff, 0xf18); // sgi pending clear
  _dist.write<Unsigned32>(0xffffffff, 0xf1c); // sgi pending clear

  for (unsigned g = 0; g < 32; g += 4)
    {
      Mword v = 0;
      if (Config_tz_sec)
        {
          unsigned b = (sec_irqs >> g) & 0xf;

          for (int i = 0; i < 4; ++i)
            if (b & (1 << i))
              v |= 0x40 << (i * 8);
            else
              v |= 0xa0 << (i * 8);
        }
      else
        v = 0xa0a0a0a0;

      _dist.write<Unsigned32>(v, GICD_IPRIORITYR + g);
    }
}

PUBLIC inline
void
Gic_dist::sync_rwp(V2)
{}

//-------------------------------------------------------------------
IMPLEMENTATION [have_arm_gicv3]:

#include "cpu.h"
#include "poll_timeout_counter.h"
#include "processor.h"
#include "warn.h"

PUBLIC inline
unsigned
Gic_dist::hw_nr_lpis()
{
  Typer_v3 t = typer<Typer_v3>();
  if (!t.lpis())
    return 0;

  Unsigned64 num_intids = 1ULL << (t.id_bits() + 1);
  if (num_intids <= Lpi_intid_base)
    return 0;

  unsigned num_lpis = t.num_lpis();
  if (num_lpis == 0)
    // Number of supported LPIs is indicated by GICD_TYPER.IDbits.
    return num_intids - Lpi_intid_base;

  // Number of supported LPIs = 2^(num_lpis+1)
  return 1U << (num_lpis + 1);
}

PUBLIC static inline
Unsigned64
Gic_dist::cpu_to_irouter_entry(Cpu_number cpu)
{
  auto phys_id = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());
  return Unsigned64{phys_id & 0xff000000} << 8 | (phys_id & 0xffffff);
}

PUBLIC inline
void
Gic_dist::set_cpu(Mword pin, Unsigned64 irouter_entry, V3)
{
  if (pin < 32) // GICD_IROUTER<0..31> are reserved
    return;
  _dist.write_non_atomic<Unsigned64>(irouter_entry, GICD_IROUTER + 8 * pin);
}

PUBLIC inline NEEDS["cpu.h"]
void
Gic_dist::init_targets(unsigned max, V3)
{
  Unsigned64 t = Cpu::mpidr() & 0xff00ffffffULL;

  for (unsigned i = 32; i < max; ++i)
    _dist.write_non_atomic<Unsigned64>(t, GICD_IROUTER + 8 * i);
}

PRIVATE
void
Gic_dist::igroup_init(V3, unsigned num)
{
  for (unsigned i = 32; i < num; i += 32)
    _dist.write<Unsigned32>(~0u, GICD_IGROUPR + i / 8);
}

PUBLIC inline
void
Gic_dist::enable(V3)
{
  Unsigned32 dist_enable = GICD_CTRL_ENGR1 | GICD_CTRL_ENGR1A | GICD_CTRL_ARE_NS;
  _dist.write<Unsigned32>(dist_enable, GICD_CTRL);
  sync_rwp(V3());
}

PUBLIC inline
void
Gic_dist::cpu_init(V3)
{}

PUBLIC inline NEEDS["poll_timeout_counter.h", "processor.h", "warn.h"]
void
Gic_dist::sync_rwp(V3)
{
  L4::Poll_timeout_counter i(1U << 27); // ~134ms @ 1GHz
  while (i.test(_dist.read<Unsigned32>(GICD_CTRL) & GICD_CTRL_RWP))
    Proc::pause();

  if (EXPECT_FALSE(i.timed_out()))
    WARNX(Error, "GICD: RWP timed out!\n");
}

//-------------------------------------------------------------------
IMPLEMENTATION [have_arm_gicv3 && mp]:

PUBLIC inline
Unsigned32
Gic_dist::irouter(unsigned num)
{
  return _dist.read_non_atomic<Unsigned64>(GICD_IROUTER + num * 8);
}

//-------------------------------------------------------------------
IMPLEMENTATION:

#include "l4_types.h"
#include "lock_guard.h"

PUBLIC explicit inline
Gic_dist::Gic_dist(void *dist_base)
  : _dist(dist_base)
{}

PUBLIC template <typename TYPER> inline
TYPER
Gic_dist::typer()
{
  return TYPER(_dist.read<Unsigned32>(GICD_TYPER));
}

PUBLIC inline
unsigned
Gic_dist::hw_nr_pins()
{
  return (typer<Typer>().num_spis() + 1) * 32;
}

PUBLIC inline
bool
Gic_dist::has_sec_ext()
{
  return typer<Typer>().security_extn();
}

PUBLIC inline
void
Gic_dist::softint(Unsigned32 sgi)
{
  _dist.write<Unsigned32>(sgi, GICD_SGIR);
}

PUBLIC inline
template<typename VERSION>
void
Gic_dist::disable(VERSION)
{
  _dist.write<Unsigned32>(0, GICD_CTRL);
  sync_rwp(VERSION());
}

PUBLIC inline
void
Gic_dist::init_prio(unsigned from, unsigned to)
{
  for (unsigned i = from; i < to; i += 4)
    _dist.write<Unsigned32>(0xa0a0a0a0, GICD_IPRIORITYR + i);
}

PUBLIC inline
void
Gic_dist::init_regs(unsigned from, unsigned to)
{
  for (unsigned i = from; i < to; i += 32)
    _dist.write<Unsigned32>(0xffffffff, GICD_ICENABLER + i * 4 / 32);
}

PUBLIC inline NEEDS["l4_types.h", "lock_guard.h"]
int
Gic_dist::set_mode(Mword pin, Irq_chip::Mode m)
{
  if (!m.set_mode())
    return 0;

  if (pin < 16)
    return -L4_err::EInval;

  unsigned v = 0;
  switch (m.flow_type())
    {
    case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high:
    case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_low:
      break;
    case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high:
    case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_low:
      v = 2;
      break;
    default:
      return -L4_err::EInval;
    };

  unsigned shift = (pin & 15) * 2;

  auto guard = lock_guard(_lock);
  _dist.modify<Unsigned32>(v << shift, 3 << shift, GICD_ICFGR + (pin >> 4) * 4);

  return 0;
}

PUBLIC inline
bool
Gic_dist::is_edge_triggered(Mword pin) const
{
  if (pin < 16)
    return false;

  Unsigned32 v = _dist.read<Unsigned32>(GICD_ICFGR + (pin >> 4) * 4);
  return (v >> ((pin & 15) * 2)) & 2;
}

PUBLIC inline NEEDS["lock_guard.h"]
void
Gic_dist::setup_pin_grp0(Mword pin)
{
  unsigned shift = (pin & 3) * 8;
  auto guard = lock_guard(_lock);

  _dist.clear<Unsigned32>(1UL << (pin & 0x1f),
                          GICD_IGROUPR + (pin & ~0x1f) / 8);

  _dist.modify<Unsigned32>(0x40 << shift, 0xff << shift, GICD_IPRIORITYR + (pin & ~3));
}

PUBLIC inline NEEDS["lock_guard.h"]
void
Gic_dist::set_pending_irq(unsigned idx, Unsigned32 val)
{
  if (idx < 32)
    {
      Address o = idx * 4;
      auto guard = lock_guard(_lock);
      Unsigned32 v = val & _dist.read<Unsigned32>(o + GICD_IGROUPR);
      _dist.write<Unsigned32>(v, o + GICD_ISPENDR);
    }
}

PUBLIC
template<typename VERSION>
void
Gic_dist::disable_irq(VERSION, unsigned irq)
{
  _dist.write<Unsigned32>(1 << (irq % 32), GICD_ICENABLER + (irq / 32) * 4);
  sync_rwp(VERSION());
}

PUBLIC inline
void
Gic_dist::enable_irq(unsigned irq)
{ _dist.write<Unsigned32>(1 << (irq % 32), GICD_ISENABLER + (irq / 32) * 4); }

PUBLIC
template<typename VERSION>
unsigned
Gic_dist::init(VERSION, unsigned cpu_prio, int nr_pins_override = -1)
{
  disable(VERSION());

  unsigned num = hw_nr_pins();
  if (nr_pins_override >= 0)
    num = static_cast<unsigned>(nr_pins_override);

  if (!Config_mxc_tzic)
    {
      for (unsigned i = 32; i < num; i += 16)
        _dist.write<Unsigned32>(0, GICD_ICFGR + i * 4 / 16);
    }

  init_prio(32, num);
  init_regs(32, num);
  igroup_init(VERSION(), num);

  enable(VERSION());

  // Initialize interrupt targets last, since for the GICv3 affinity routing
  // must be enabled before interrupt affinities (targets) can be assigned.
  init_targets(num, VERSION());

  if (Config_mxc_tzic)
    {
      _dist.write<Unsigned32>(0x0, MXC_TZIC_SYNCCTRL);
      _dist.write<Unsigned32>(cpu_prio, MXC_TZIC_PRIOMASK);
    }

  return num;
}

PUBLIC inline
void
Gic_dist::irq_prio(unsigned irq, unsigned prio)
{
  _dist.write<Unsigned8>(prio, GICD_IPRIORITYR + irq);
}

PUBLIC inline
unsigned
Gic_dist::irq_prio(unsigned irq)
{
  return _dist.read<Unsigned8>(GICD_IPRIORITYR + irq);
}

