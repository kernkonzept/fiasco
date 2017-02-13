// --------------------------------------------------------------------------
INTERFACE [arm && pf_bcm283x && pf_bcm283x_rpi1]:

// HINT: This is drivers/clocksource/bcm2835_timer.c

#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static unsigned irq() { return 3; }

private:
  enum
  {
    CS  = 0,
    CLO = 4,
    CHI = 8,
    C0  = 0xc,
    C1  = 0x10,
    C2  = 0x14,
    C3  = 0x18,

    Timer_nr = 3,
    Interval = 1000,
  };

  static Static_object<Timer> _timer;
};

// --------------------------------------------------------------------------
INTERFACE [arm && pf_bcm283x && arm_generic_timer]:

#include "per_cpu_data.h"

EXTENSION class Timer
{
private:
  static Per_cpu<Unsigned64> _initial_hi_val;
};

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_bcm283x && arm_generic_timer]:

IMPLEMENT inline
void Timer::bsp_init(Cpu_number)
{
  // should probably read the hi val here?
}

PUBLIC static inline
unsigned Timer::irq()
{
  switch (Gtimer::Type)
    {
    case Generic_timer::Physical: return 1; // use the non-secure IRQ
    case Generic_timer::Virtual:  return 3;
    case Generic_timer::Hyp:      return 2;
    };
}

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && pf_bcm283x && pf_bcm283x_rpi1]:

#include "config.h"
#include "kip.h"
#include "kmem.h"
#include "mem_layout.h"

Static_object<Timer> Timer::_timer;

PRIVATE inline
void
Timer::set_next()
{
  write<Mword>(read<Mword>(CLO) + Interval, C0 + Timer_nr * 4);
}

PUBLIC
Timer::Timer(Address base) : Mmio_register_block(base)
{
  set_next();
}

IMPLEMENT
void Timer::init(Cpu_number)
{ _timer.construct(Kmem::mmio_remap(Mem_layout::Timer_phys_base)); }

PUBLIC static inline NEEDS[Timer::set_next]
void
Timer::acknowledge()
{
  _timer->set_next();
  _timer->write<Mword>(1 << Timer_nr, CS);
}

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 wakeup)
{
  (void)wakeup;
}

IMPLEMENT inline NEEDS["kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  return Kip::k()->clock;
}
