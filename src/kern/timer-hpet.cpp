INTERFACE [hpet_timer]:

#include "irq_chip.h"

EXTENSION class Timer
{
  static int hpet_irq;
};

IMPLEMENTATION [hpet_timer]:

#include "config.h"
#include "cpu.h"
#include "hpet.h"
#include "logdefs.h"
#include "pit.h"
#include "std_macros.h"

#include <cstdio>

int Timer::hpet_irq;

PUBLIC static inline int Timer::irq() { return hpet_irq; }

PUBLIC static inline NEEDS["irq_chip.h"]
Irq_chip::Mode Timer::irq_mode()
{ return Irq_chip::Mode::F_level_low; }

IMPLEMENT
void
Timer::init(Cpu_number)
{
  hpet_irq = -1;
  if (!Hpet::init())
    return;

  hpet_irq = Hpet::int_num();
  if (hpet_irq == 0 && Hpet::int_avail(2))
    hpet_irq = 2;

  if (Config::Scheduler_one_shot)
    {
      // tbd
    }
  else
    {
      // setup hpet for periodic here
    }

  if (!Config::Scheduler_one_shot)
    // from now we can save energy in getchar()
    Config::getchar_does_hlt_works_ok = Config::hlt_works_ok;

  Hpet::enable();
  Hpet::dump();

  printf("Using HPET timer on IRQ %d (%s Mode) for scheduling\n",
         hpet_irq,
         Config::Scheduler_one_shot ? "One-Shot" : "Periodic");
}

PUBLIC static inline
void
Timer::acknowledge()
{}

static
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{
}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64 /*wakeup*/)
{
}
