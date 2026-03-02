INTERFACE:

#include <cxx/bitfield>
#include <l4_types.h>

class Gic_redist_regs
{
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

  struct Ctrl
  {
    Unsigned32 raw;
    Ctrl() = default;
    explicit Ctrl(Unsigned32 v) : raw(v) {}
    CXX_BITFIELD_MEMBER          ( 0,  0, enable_lpis, raw);
    CXX_BITFIELD_MEMBER          ( 3,  3, rwp,         raw);
  };

  struct Typer
  {
    Unsigned64 raw;
    Typer() = default;
    explicit Typer(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER_RO( 0,  0, plpis, raw);
    CXX_BITFIELD_MEMBER_RO( 1,  1, vlpis, raw);
    CXX_BITFIELD_MEMBER_RO( 4,  4, last, raw);
    CXX_BITFIELD_MEMBER_RO( 8, 23, processor_nr, raw);
    CXX_BITFIELD_MEMBER_RO(32, 63, affinity, raw);
  };
};

