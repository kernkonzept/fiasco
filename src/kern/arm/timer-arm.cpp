INTERFACE [arm]:

#include "irq_chip.h"

EXTENSION class Timer
{
public:
  static Irq_chip::Mode irq_mode()
  { return Irq_chip::Mode::F_raising_edge; }

private:
  static inline void update_one_shot(Unsigned64 wakeup);
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && vcache]:

#include "mem_unit.h"

PRIVATE static inline NEEDS["mem_unit.h"]
void
Timer::kipclock_cache()
{
  Mem_unit::clean_dcache((void *)&Kip::k()->clock);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !vcache]:

PRIVATE static inline
void
Timer::kipclock_cache()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "config.h"
#include "globals.h"
#include "kip.h"
#include "watchdog.h"

IMPLEMENT inline NEEDS["kip.h"]
void
Timer::init_system_clock()
{
  Kip::k()->clock = 0;
}

IMPLEMENT inline NEEDS["config.h", "globals.h", "kip.h", "watchdog.h", Timer::kipclock_cache]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      Kip::k()->clock += Config::Scheduler_granularity;
      kipclock_cache();
      Watchdog::touch();
    }
}

IMPLEMENT inline NEEDS[Timer::update_one_shot, "config.h"]
void
Timer::update_timer(Unsigned64 wakeup)
{
  if (Config::Scheduler_one_shot)
    update_one_shot(wakeup);
}
