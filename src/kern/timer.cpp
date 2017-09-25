INTERFACE:

#include "initcalls.h"
#include "l4_types.h"

class Timer
{
public:
  /**
   * Static constructor for the interval timer.
   *
   * The implementation is platform specific. Two x86 implementations
   * are timer-pit and timer-rtc.
   */
  static void init(Cpu_number cpu) FIASCO_INIT_CPU_AND_PM;

  /**
   * Initialize the system clock.
   */
  static void init_system_clock();

  /**
   * Advances the system clock.
   */
  static void update_system_clock(Cpu_number cpu);

  /**
   * Get the current system clock.
   */
  static Unsigned64 system_clock();

  /**
   * reprogram the one-shot timer to the next event.
   */
  static void update_timer(Unsigned64 wakeup);

  /**
   * enable the timer
   */
  static void enable();

  static void master_cpu(Cpu_number cpu) { _cpu = cpu; }

private:
  static Cpu_number _cpu;
};


IMPLEMENTATION:

Cpu_number Timer::_cpu;

IMPLEMENT_DEFAULT
void
Timer::enable()
{}
