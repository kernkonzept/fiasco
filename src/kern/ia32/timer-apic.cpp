IMPLEMENTATION [apic_timer]:

#include <cstdio>

#include "apic.h"
#include "config.h"
#include "cpu.h"
#include "logdefs.h"
#include "pit.h"
#include "std_macros.h"
#include "warn.h"

// no IRQ line for the LAPIC
PUBLIC static inline int Timer::irq() { return -1; }

IMPLEMENT
void
Timer::init(Cpu_number)
{
  Apic::timer_assign_irq_vector(Config::Apic_timer_vector);

  if constexpr (Config::Scheduler_one_shot)
    {
      Apic::timer_set_one_shot();
      Apic::timer_reg_write(Apic::Timer_max);
    }
  else
    {
      Apic::timer_set_periodic();
      Apic::timer_reg_write(Apic::us_to_apic(Config::Scheduler_granularity));
    }

  // make sure that PIT does pull its interrupt line
  Pit::done();

  if constexpr (!Config::Scheduler_one_shot)
    // from now we can save energy in getchar()
    Config::getchar_does_hlt_works_ok = false && Config::hlt_works_ok;

  if (Warn::is_enabled(Info))
    printf ("Using the Local APIC timer on vector %x (%s Mode) for scheduling\n",
            static_cast<unsigned>(Config::Apic_timer_vector),
            Config::Scheduler_one_shot ? "One-Shot" : "Periodic");

}

PUBLIC static inline
void
Timer::acknowledge()
{}

static
void
Timer::update_one_shot(Unsigned64 wakeup)
{
  Unsigned32 apic;
  Unsigned64 now = system_clock();
  if (EXPECT_FALSE(wakeup <= now))
    // already expired
    apic = 1;
  else
    {
      Unsigned64 delta = wakeup - now;
      if (delta < Config::One_shot_min_interval_us)
	apic = Apic::us_to_apic(Config::One_shot_min_interval_us);
      else if (delta > Config::One_shot_max_interval_us)
	apic = Apic::us_to_apic(Config::One_shot_max_interval_us);
      else
        apic = Apic::us_to_apic(delta);

      if (EXPECT_FALSE (apic < 1))
	// timeout too small
	apic = 1;
    }

  Apic::timer_reg_write(apic);
}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64 wakeup)
{
  if constexpr (Config::Scheduler_one_shot)
    update_one_shot(wakeup);
}
