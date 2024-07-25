INTERFACE:

#include "l4_types.h"

class Clock_base
{
};

/**
 * Per-CPU clock used for determining how long a thread has been running.
 *
 * The simple implementation just uses the KIP clock. Other implementations
 * use a cheap per-CPU timer, for example the TSC on ia32.
 */
class Clock : public Clock_base
{
public:
  typedef Unsigned64 Time;

  Clock(Cpu_number cpu);

  /**
   * Determine the number of clock cycles sine the last call of this function.
   */
  Time delta();

  /**
   * Convert clock cycles into microseconds.
   *
   * This operation needs some CPU cycles, hence this operation is only
   * performed if necessary, that is, when requesting the summary information.
   *
   * \param t  Clock counter value.
   *
   * \return Corresponding microseconds.
   */
  Cpu_time us(Time t);


private:
  Counter _last_value = read_counter();

  /**
   * Read the current clock counter.
   *
   * \return Clock counter value.
   */
  Counter read_counter() const;
};

IMPLEMENTATION:

IMPLEMENT_DEFAULT inline
Clock::Clock(Cpu_number)
{}


IMPLEMENT inline
Clock::Time
Clock::delta()
{
  Counter t = read_counter();
  Counter r = t - _last_value;
  _last_value = t;
  return Time(r);
}
