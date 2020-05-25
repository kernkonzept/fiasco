INTERFACE:

#include "mmio_register_block.h"


class Gic_cpu_v2
{
private:
  Mmio_register_block _cpu;

public:
  enum
  {
    GICC_CTRL         = 0x00,
    GICC_PMR          = 0x04,
    GICC_BPR          = 0x08,
    GICC_IAR          = 0x0c,
    GICC_EOIR         = 0x10,
    GICC_RPR          = 0x14,
    GICC_HPPIR        = 0x18,

    Size              = 0x2000,

    GICC_CTRL_ENABLE_GRP0    = 1 << 0,
    GICC_CTRL_ENABLE_GRP1    = 1 << 1,
    GICC_CTRL_ENABLE         = GICC_CTRL_ENABLE_GRP0,
    GICC_CTRL_FIQEn          = 1 << 3,

    Cpu_prio_val      = 0xf0,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm_em_tz]:

EXTENSION class Gic_cpu_v2 { static constexpr bool Config_tz_sec = true; };

// ------------------------------------------------------------------------
INTERFACE [!arm_em_tz]:

EXTENSION class Gic_cpu_v2 { static constexpr bool Config_tz_sec = false; };

//-------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
void
Gic_cpu_v2::pmr(unsigned prio)
{
  _cpu.write<Unsigned32>(prio, GICC_PMR);
}

PUBLIC inline
void
Gic_cpu_v2::enable()
{
  _cpu.write<Unsigned32>(GICC_CTRL_ENABLE | (Config_tz_sec ? GICC_CTRL_FIQEn : 0),
                         GICC_CTRL);
  pmr(Cpu_prio_val);
}

PUBLIC inline
void
Gic_cpu_v2::disable()
{
  _cpu.write<Unsigned32>(0, GICC_CTRL);
}

PUBLIC explicit inline
Gic_cpu_v2::Gic_cpu_v2(Address cpu_base)
  : _cpu(cpu_base)
{}

PUBLIC inline
void
Gic_cpu_v2::ack(Unsigned32 irq)
{
  _cpu.write<Unsigned32>(irq, GICC_EOIR);
}

PUBLIC inline
Unsigned32
Gic_cpu_v2::iar()
{
  return _cpu.read<Unsigned32>(GICC_IAR);
}

PUBLIC inline
unsigned
Gic_cpu_v2::pmr()
{ return _cpu.read<Unsigned32>(GICC_PMR); }

