INTERFACE:

#include "l4_types.h"

class Clock_base
{
};

class Clock : public Clock_base
{
public:
  typedef Unsigned64 Time;

  Clock(Cpu_number cpu);

  Time delta();

  Cpu_time us(Time t);


private:
  Counter _last_value = read_counter();

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
