// --------------------------------------------------------------------------
INTERFACE [arm && bcm2835]:

#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static unsigned irq() { return 3; }

private:
  enum {
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

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && bcm2835]:

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
