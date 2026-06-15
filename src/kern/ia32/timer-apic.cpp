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
Timer::init(Cpu_number cpu)
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

  if (cpu == Cpu_number::boot_cpu())
    printf("Local APIC timer on vector %x in %s mode\n",
           static_cast<unsigned>(Config::Apic_timer_vector),
           Config::Scheduler_one_shot ? "one-shot" : "periodic");
}

PUBLIC static inline
void
Timer::acknowledge()
{}

IMPLEMENT_OVERRIDE
void
Timer::update_timer(Unsigned64 wakeup)
{
  if constexpr (Config::Scheduler_one_shot)
    {
      Unsigned32 apic;
      Unsigned64 now = system_clock();
      if (wakeup == Infinite_timeout) [[unlikely]]
        apic = 0; // Stop timer for infinite timeout.
      else if (wakeup <= now) [[unlikely]]
        apic = Apic::Timer_min;
      else
        {
          apic = Apic::us_to_apic(wakeup - now);
          if (apic < Apic::Timer_min) [[unlikely]]
            apic = Apic::Timer_min;
        }

      Apic::timer_reg_write(apic);
    }
}
