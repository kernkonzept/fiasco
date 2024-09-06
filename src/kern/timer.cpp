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
   * Initialize the system clock for application CPUs.
   */
  static void init_system_clock_ap(Cpu_number cpu);

  /**
   * Advances the system clock.
   *
   * This is usually performed from the timer interrupt handler.
   */
  static void update_system_clock(Cpu_number cpu);

  /**
   * Get the current system clock.
   *
   * Depending on the configuration, either
   *  - read the system clock from the KIP clock value which is increased during
   *    every timer interrupt on the boot CPU (CONFIG_SYNC_CLOCK=n), or
   *  - determine the current system clock by reading a fine-grained hardware
   *    counter (TSC, ARM generic timer, etc; CONFIG_SYNC_CLOCK=y).
   *
   * The system clock increases monotonically but it may stop temporarily, for
   * instance while executing the kernel debugger.
   *
   * \note This function must not be called from JDB.
   */
  static Unsigned64 system_clock();

  /**
   * Get the current value of a kernel-internal clock not necessarily related
   * to system_clock().
   *
   * This clock is intended for cases in which a continuous clock is required
   * but the system clock might have stopped. Reading this clock has no side
   * effects.
   *
   * \note This function is usually called from JDB. It must not be used if the
   *       time is related to userland events.
   * \note This function is only implemented with certain configurations.
   */
  static Unsigned64 aux_clock_unstopped();

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
INTERFACE[sync_clock && test_support_code]:

#include "global_data.h"

EXTENSION class Timer
{
private:
  static Global_data<Unsigned64> _system_clock_at_last_timer_tick;
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


//----------------------------------------------------------------------------
IMPLEMENTATION:

#include "kip.h"

Cpu_number Timer::_cpu;

IMPLEMENT_DEFAULT
void
Timer::init_system_clock()
{
  Kip::k()->set_clock(0);
}

IMPLEMENT_DEFAULT
void
Timer::init_system_clock_ap(Cpu_number)
{}

IMPLEMENT_DEFAULT
void
Timer::enable()
{}

IMPLEMENT_DEFAULT inline NEEDS["kip.h"]
Unsigned64
Timer::system_clock()
{
  return Kip::k()->clock();
}

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

//----------------------------------------------------------------------------
IMPLEMENTATION[sync_clock && !test_support_code]:

IMPLEMENT inline
void
Timer::update_system_clock(Cpu_number)
{}

//----------------------------------------------------------------------------
IMPLEMENTATION[sync_clock && test_support_code]:

DEFINE_GLOBAL Global_data<Unsigned64> Timer::_system_clock_at_last_timer_tick;

PUBLIC static inline
Unsigned64
Timer::system_clock_at_last_timer_tick()
{ return _system_clock_at_last_timer_tick; }

/**
 * This is a compromise: In this configuration, the KIP clock is not updated
 * anymore but certain unit tests need an indicator if the timer interrupt was
 * triggered.
 */
IMPLEMENT inline
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    _system_clock_at_last_timer_tick = system_clock();
}
