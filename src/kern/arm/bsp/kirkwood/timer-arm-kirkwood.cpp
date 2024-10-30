// --------------------------------------------------------------------------
INTERFACE [arm && pf_kirkwood]:

#include "kmem_mmio.h"
#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static unsigned irq() { return 1; }

private:
  enum {
    Control_Reg  = 0x300,
    Reload0_Reg  = 0x310,
    Timer0_Reg   = 0x314,
    Reload1_Reg  = 0x318,
    Timer1_Reg   = 0x31c,

    Bridge_cause = 0x110,
    Bridge_mask  = 0x114,

    Timer0_enable = 1 << 0,
    Timer0_auto   = 1 << 1,

    Timer0_bridge_num = 1 << 1,
    Timer1_bridge_num = 1 << 2,

    Reload_value = 200000,
  };

  static Static_object<Timer> _timer;
};

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && pf_kirkwood]:

#include "io.h"

Static_object<Timer> Timer::_timer;

PUBLIC
Timer::Timer()
: Mmio_register_block(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x400))
{
  // Disable timer
  write<Unsigned32>(0, Control_Reg);

  // Set current timer value and reload value
  write<Unsigned32>(Reload_value, Timer0_Reg);
  write<Unsigned32>(Reload_value, Reload0_Reg);

  modify<Unsigned32>(Timer0_enable | Timer0_auto, 0, Control_Reg);
  modify<Unsigned32>(Timer0_bridge_num, 0, Bridge_mask);
}

IMPLEMENT
void Timer::init(Cpu_number)
{
  _timer.construct();
}

PUBLIC static inline NEEDS["io.h"]
void
Timer::acknowledge()
{
  _timer->modify<Unsigned32>(0, Timer0_bridge_num, Bridge_cause);
}
