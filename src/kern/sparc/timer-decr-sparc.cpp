INTERFACE [sparc]:

#include "irq_chip.h"
#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

private:
  enum
  {
    Scaler        = 0,
    Scaler_reload = 4,
    Config        = 8,

    Timer_nr = 0,

    Timer_counter = 0x10 + Timer_nr * 0x10 + 0x0,
    Timer_reload  = 0x10 + Timer_nr * 0x10 + 0x4,
    Timer_control = 0x10 + Timer_nr * 0x10 + 0x8,
    Timer_latch   = 0x10 + Timer_nr * 0x10 + 0xc,

  };
  static Static_object<Timer> _timer;
};

IMPLEMENTATION [sparc]:

#include "cpu.h"
#include "config.h"
#include "globals.h"
#include "kip.h"
#include "kmem.h"
#include "warn.h"

#include <cstdio>

Static_object<Timer> Timer::_timer;

PUBLIC
Timer::Timer() : Mmio_register_block(Kmem::mmio_remap(0x80000300))
{
  r<32>(Scaler) = 0;
  r<32>(Scaler_reload) = 0;

  Unsigned32 c = r<32>(Config);
  assert(((c >> 8) & 1) == 1);

  printf("Timers: num=%d irq=%d SI=%d [%08x]\n",
         c & 7, irq_num(), (c >> 8) & 1, c);

  r<32>(Config).clear(0x7f << 16);
  r<32>(Timer_reload) = 100000;
  r<32>(Timer_control) = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);

  r<32>(Config).set(1 << (16 + Timer_nr));
}

PRIVATE
unsigned
Timer::irq_num()
{ return (r<32>(Config) >> 3) & 0x1f; }

PUBLIC static unsigned
Timer::irq()
{ return _timer->irq_num() + Timer_nr; }

IMPLEMENT
void
Timer::init(Cpu_number)
{
  _timer.construct();
}

PUBLIC static inline
void
Timer::acknowledge()
{ }

//IMPLEMENT inline NEEDS ["decrementer.h"]
//void
//Timer::enable()
//{
//  Decrementer::d()->enable();
//}

//IMPLEMENT inline NEEDS ["decrementer.h"]
//void
//Timer::disable()
//{
//  Decrementer::d()->disable();
//}

IMPLEMENT inline NEEDS ["kip.h"]
void
Timer::init_system_clock()
{
  Kip::k()->clock = 0;
}

IMPLEMENT inline NEEDS ["globals.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  return Kip::k()->clock;
}

IMPLEMENT inline NEEDS ["config.h", "kip.h"]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    Kip::k()->clock += Config::Scheduler_granularity;
}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64 wakeup)
{
  (void)wakeup;
  //if (Config::Scheduler_one_shot)
  //  update_one_shot(wakeup);
}
