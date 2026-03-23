
INTERFACE:

#include "timeout.h"

class Timeslice_timeout : public Timeout
{
  friend class Test_timeslice_timeout;
};

IMPLEMENTATION:

#include <cassert>
#include "globals.h"
#include "sched_context.h"
#include "std_macros.h"
#include <options.h>

/* Initialize global variable timeslice_timeout */
DEFINE_PER_CPU Per_cpu<Timeout *> timeslice_timeout;
DEFINE_PER_CPU static Per_cpu<Timeslice_timeout> the_timeslice_timeout(Per_cpu_data::Cpu_num);

PUBLIC
Timeslice_timeout::Timeslice_timeout(Cpu_number cpu)
{
  timeslice_timeout.cpu(cpu) = this;
}


/**
 * Timeout expiration callback function
 * \return Reschedule::Yes (always forces a reschedule).
 */
PRIVATE
Reschedule
Timeslice_timeout::expired() override
{
  Sched_context::Ready_queue &rq = Sched_context::rq.current();
  Sched_context *sched = rq.current_sched();

  if (sched)
    {
      sched->replenish();
      rq.requeue(sched);
      rq.invalidate_sched();
    }

  return Reschedule::Yes; // Force reschedule
}
