INTERFACE [arm]:

#include <mmio_register_block.h>

class Scu : public Mmio_register_block
{
private:
  enum
  {
    Control      = 0x0,
    Config       = 0x4,
    Power_status = 0x8,
    Inv          = 0xc,
    SAC          = 0x50,
    SNSAC        = 0x54,
  };

public:
  enum
  {
    Control_ic_standby     = 1 << 6,
    Control_scu_standby    = 1 << 5,
    Control_force_port0    = 1 << 4,
    Control_spec_linefill  = 1 << 3,
    Control_ram_parity     = 1 << 2,
    Control_addr_filtering = 1 << 1,
    Control_enable         = 1 << 0,
  };

  void reset() const { write<Mword>(0xffffffff, Inv); }

  void enable(Mword bits = 0) const
  {
    Unsigned32 ctrl = read<Unsigned32>(Control);
    if (!(ctrl & Control_enable))
      write<Unsigned32>(ctrl | bits | Control_enable, Control);
  }

  Mword config() const { return read<Mword>(Config); }
};

//-------------------------------------------------------------------------
INTERFACE [arm && !bsp_cpu]:

EXTENSION class Scu
{
public:
  enum
  {
    Bsp_enable_bits = 0,
    Available = 1,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_em_tz && mptimer]:

EXTENSION class Scu
{
  enum : Mword { SNSAC_value = 0 };
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_em_tz && !mptimer]:

EXTENSION class Scu
{
  enum : Mword { SNSAC_value = 0xfff };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_mpcore || (arm_cortex_a9 && !arm_em_tz))]:

PUBLIC explicit
Scu::Scu(Address base)
: Mmio_register_block(base)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_cortex_a9 && arm_em_tz]:

PUBLIC explicit
Scu::Scu(Address base)
: Mmio_register_block(base)
{
  write<Mword>(SNSAC_value, SNSAC);
}
