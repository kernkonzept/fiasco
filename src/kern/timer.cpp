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


//----------------------------------------------------------------------------
INTERFACE[jdb]:

EXTENSION class Timer
{
public:
  /**
   * Reduce the timer frequency (when entering JDB).
   */
  static void switch_freq_jdb();

  /**
   * Return the timer frequency to normal (when leaving JDB).
   */
  static void switch_freq_system();
};


IMPLEMENTATION:

Cpu_number Timer::_cpu;

IMPLEMENT_DEFAULT
void
Timer::enable()
{}


//----------------------------------------------------------------------------
IMPLEMENTATION[jdb]:

IMPLEMENT_DEFAULT
void
Timer::switch_freq_jdb()
{}

IMPLEMENT_DEFAULT
void
Timer::switch_freq_system()
{}
