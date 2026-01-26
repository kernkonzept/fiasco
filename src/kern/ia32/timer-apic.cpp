IMPLEMENTATION [apic_timer]:

#include <cstdio>
#include <cxx/conditionals>

#include "apic.h"
#include "config.h"
#include "cpu.h"
#include "logdefs.h"
#include "pit.h"
#include "std_macros.h"
#include "warn.h"

// No global system interrupt for the Local APIC timer
PUBLIC static inline
unsigned Timer::irq() { return ~0U; }

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

  WARNX(Info,"Using the Local APIC timer on vector %x (%s Mode) for scheduling\n",
             static_cast<unsigned>(Config::Apic_timer_vector),
             cxx::const_ite<Config::Scheduler_one_shot>("One-Shot", "Periodic"));

}

PUBLIC static inline
void
Timer::acknowledge()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION[apic_timer && one_shot]:

IMPLEMENT
void
Timer::update_timer(Unsigned64 wakeup)
{
  Unsigned32 apic;
  Unsigned64 now = system_clock();
  if (EXPECT_FALSE(wakeup == Infinite_timeout))
    apic = 0; // Stop timer for infinite timeout.
  else if (EXPECT_FALSE(wakeup <= now))
    apic = Apic::Timer_min;
  else
    {
      apic = Apic::us_to_apic(wakeup - now);
      if (EXPECT_FALSE(apic < Apic::Timer_min))
        apic = Apic::Timer_min;
    }

  Apic::timer_reg_write(apic);
}
