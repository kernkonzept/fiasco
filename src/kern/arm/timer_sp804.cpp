// --------------------------------------------------------------------------
INTERFACE [arm]:

#include "kmem.h"
#include "mmio_register_block.h"

class Timer_sp804 : private Mmio_register_block
{
public:
  enum {
    Refclk = 0,
    Timclk = 1,

    Load   = 0x00,
    Value  = 0x04,
    Ctrl   = 0x08,
    Intclr = 0x0c,

    Interval = 1000,

    Ctrl_ie        = 1 << 5,
    Ctrl_periodic  = 1 << 6,
    Ctrl_enable    = 1 << 7,
  };

  Timer_sp804(Address virt) : Mmio_register_block(virt) {}
  void disable() const { write<Mword>(0, Ctrl); }
  void enable(Mword flags) const
  { write<Mword>(Ctrl_enable | flags, Ctrl); }
  void reload_value(Mword v) const { write(v, Load); }
  void counter_value(Mword v) const { write(v, Value); }
  Mword counter() const { return read<Mword>(Value); }
  void irq_clear() const { write<Mword>(0, Intclr); }
};


