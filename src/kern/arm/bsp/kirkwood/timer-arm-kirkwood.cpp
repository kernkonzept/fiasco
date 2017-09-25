// --------------------------------------------------------------------------
INTERFACE [arm && kirkwood]:

#include "kmem.h"
#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static unsigned irq() { return 1; }

private:
  enum {
    Control_Reg  = 0x20300,
    Reload0_Reg  = 0x20310,
    Timer0_Reg   = 0x20314,
    Reload1_Reg  = 0x20318,
    Timer1_Reg   = 0x2031c,

    Bridge_cause = 0x20110,
    Bridge_mask  = 0x20114,

    Timer0_enable = 1 << 0,
    Timer0_auto   = 1 << 1,

    Timer0_bridge_num = 1 << 1,
    Timer1_bridge_num = 1 << 2,

    Reload_value = 200000,
  };

  static Static_object<Timer> _timer;
};

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && kirkwood]:

#include "config.h"
#include "kip.h"
#include "io.h"

Static_object<Timer> Timer::_timer;

PUBLIC
Timer::Timer()
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timer_phys_base))
{
  // Disable timer
  write(0, Control_Reg);

  // Set current timer value and reload value
  write<Mword>(Reload_value, Timer0_Reg);
  write<Mword>(Reload_value, Reload0_Reg);

  modify<Mword>(Timer0_enable | Timer0_auto, 0, Control_Reg);
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

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{
}

IMPLEMENT inline NEEDS["config.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  return Kip::k()->clock;
}
