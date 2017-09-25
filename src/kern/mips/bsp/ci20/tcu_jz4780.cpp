INTERFACE:

#include "mmio_register_block.h"

class Tcu_jz4780
{
public:
  enum R32 : Address
  {
    TSR        = 0x1c,
    TFR        = 0x20,
    TFSR       = 0x24,
    TFCR       = 0x28,
    TSSR       = 0x2c,
    TMR        = 0x30,
    TMSR       = 0x34,
    TMCR       = 0x38,
    TSCR       = 0x3c,
    OSTDR      = 0xe0,
    OSTCNTL    = 0xe4,
    OSTCNTH    = 0xe8,
    TSTR       = 0xf0,
    TSTSR      = 0xf4,
    TSTCR      = 0xf8,
    CSTCNTHBUF = 0xfc,
  };

  enum R16 : Address
  {
    TER    = 0x10,
    TESR   = 0x14,
    TECR   = 0x18,

    TDFR0  = 0x40,
    TDHR0  = 0x44,
    TCNT0  = 0x48,
    TCSR0  = 0x4c,
    OSTCSR = 0xec,
  };

  static R16 tdfr(unsigned chan)
  { return (R16)((Address)R16::TDFR0 + 0x10 * chan); }

  static R16 tdhr(unsigned chan)
  { return (R16)((Address)R16::TDHR0 + 0x10 * chan); }

  static R16 tcnt(unsigned chan)
  { return (R16)((Address)R16::TCNT0 + 0x10 * chan); }

  static R16 tcsr(unsigned chan)
  { return (R16)((Address)R16::TCSR0 + 0x10 * chan); }

  Register_block<32, void> r;

};

template<>
struct Register_block_access_size<Tcu_jz4780::R32> { enum { value = 32 }; };

template<>
struct Register_block_access_size<Tcu_jz4780::R16> { enum { value = 16 }; };

IMPLEMENTATION:

PUBLIC inline
Tcu_jz4780::Tcu_jz4780(Address mmio) : r(mmio)
{
  r[TECR] = 0x80ff;
  r[TSSR] = 0x80ff;
  r[TMSR] = 0xff80ff;
  r[TFCR] = 0xff80ff;
}
