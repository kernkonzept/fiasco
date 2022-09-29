INTERFACE:

#include "std_macros.h"
#include "initcalls.h"
#include "per_node_data.h"

class Delay
{
private:
  static Per_node_data<unsigned> count;

public:
  static void init() FIASCO_INIT;
};

IMPLEMENTATION:

#include "kip.h"
#include "processor.h"
#include "timer.h"

DECLARE_PER_NODE Per_node_data<unsigned> Delay::count;

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
  Timer::update_timer(k->clock() + 1000); // 1ms
  while (t1 == k->clock())
    {
      ++count;
      Proc::pause();
    }

  return count;
}

IMPLEMENT void
Delay::init()
{
  *count = measure();
  unsigned c2 = measure();
  if (c2 > *count)
    *count = c2;
}

/**
 * Hint: ms is actually the timer granularity, which
 *       currently happens to be milliseconds
 */
PUBLIC static void
Delay::delay(unsigned ms)
{
  Kip *k = Kip::k();
  while (ms--)
    {
      unsigned c = *count;
      while (c--)
        {
	  (void)k->clock();
	  Proc::pause();
	}
    }
}
