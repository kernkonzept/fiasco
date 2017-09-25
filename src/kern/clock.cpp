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
  Cpu_number _cpu_id;
  Counter _last_value;

  Counter read_counter() const;
};

IMPLEMENTATION:

IMPLEMENT inline
Clock::Clock(Cpu_number cpu)
  : _cpu_id(cpu), _last_value(read_counter())
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
