INTERFACE:

#include "std_macros.h"
#include "initcalls.h"
#include "global_data.h"

class Delay
{
public:
  static void init() FIASCO_INIT;

  /**
   * Determine if the delay loop is currently initializing. Better don't disturb
   * the boot CPU during that time.
   *
   * \retval true   if the calibration loop is being initialized.
   * \retval false  otherwise.
   */
  static bool initializing();

  /**
   * Wait for a certain amount of time.
   *
   * \param ms  The number of milliseconds to wait for.
   *
   * Can be used in the kernel debugger while no timer tick is available. Don't
   * expect 100% accurate delays.
   */
  static void delay(unsigned ms);

  /**
   * Wait for a certain amount of time.
   *
   * \param us  The number of microseconds to wait for.
   *
   * Can be used in the kernel debugger while no timer tick is available. Don't
   * expect 100% accurate delays.
   */
  static void udelay(unsigned us);
};

// ------------------------------------------------------------------------
IMPLEMENTATION[!sync_clock]:

#include "kip.h"
#include "mem.h"
#include "minmax.h"
#include "panic.h"
#include "processor.h"
#include "timer.h"
#include "types.h"

EXTENSION class Delay
{
private:
  static Global_data<unsigned> count;
};

DEFINE_GLOBAL Global_data<unsigned> Delay::count;

PRIVATE static
unsigned
Delay::measure()
{
  Cpu_time t1;
  unsigned count = 0;

  Kip *k = Kip::k();
  Cpu_time t = k->clock();
  Timer::update_timer(t + 1000); // 1ms
  while (t == (t1 = k->clock()))
    Proc::pause();
  Timer::update_timer(t1 + 1000); // 1ms

  // Execute code as similar to delay() as possible to get a reliable count.
  Mem::barrier();
  while (t1 == k->clock())
    {
      ++count;
      Proc::pause();
    }
  Mem::barrier();

  return count;
}

IMPLEMENT
void
Delay::init()
{
  unsigned c1 = measure();
  unsigned c2 = measure();
  count = max(c1, c2);
  Mem::mp_wmb();
  if (!count)
    panic("Delay calibration failed");
}

IMPLEMENT inline NEEDS["types.h"]
bool
Delay::initializing()
{ return access_once(&count) == 0; }

IMPLEMENT
void
Delay::delay(unsigned ms)
{
  Kip *k = Kip::k();
  while (ms--)
    {
      // 'count' was determined by waiting for the KIP counter to change from
      // one value to the next value. Hence, 'count' has KIP counter granularity
      // -- which is currently 1ms.
      unsigned c = count;

      Mem::barrier();
      while (c--)
        {
	  static_cast<void>(k->clock());
	  Proc::pause();
	}
      Mem::barrier();
    }
}

IMPLEMENT
void
Delay::udelay(unsigned us)
{
  Kip *k = Kip::k();
  // Same restriction as in Delay::delay().
  unsigned c = static_cast<unsigned long long>(count) * us / 1000;

  Mem::barrier();
  do
    {
      static_cast<void>(k->clock());
      Proc::pause();
    }
  while (c--);
  Mem::barrier();
}

// ------------------------------------------------------------------------
IMPLEMENTATION[sync_clock]:

#include "processor.h"
#include "timer.h"

IMPLEMENT
void
Delay::init()
{
  // In this configuration we use Timer::aux_clock_unstopped(), which, unlike
  // the KIP clock, updates independent of any timer tick interrupt.
}

IMPLEMENT inline
bool
Delay::initializing()
{ return false; }

IMPLEMENT
void
Delay::delay(unsigned ms)
{
  Cpu_time now = Timer::aux_clock_unstopped();
  while (Timer::aux_clock_unstopped() - now < 1000ULL * ms)
    Proc::pause();
}

IMPLEMENT
void
Delay::udelay(unsigned us)
{
  Cpu_time now = Timer::aux_clock_unstopped();
  while (Timer::aux_clock_unstopped() - now < us)
    Proc::pause();
}
