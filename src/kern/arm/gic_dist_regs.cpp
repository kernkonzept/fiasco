INTERFACE:

#include "mmio_register_block.h"
#include "spin_lock.h"

#include <cxx/bitfield>

class Gic_dist_regs
{
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

