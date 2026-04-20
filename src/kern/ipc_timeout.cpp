
INTERFACE:

#include "timeout.h"
#include "options.h"
class Receiver;

class IPC_timeout : public Timeout
{
  friend class Jdb_list_timeouts;
  friend class Timeouts_test;
};

IMPLEMENTATION:

#include "context.h"
#include "globals.h"
#include "receiver.h"
#include "thread_state.h"

/**
 * IPC_timeout constructor
 */
PUBLIC inline
IPC_timeout::IPC_timeout()
{}

/**
 * IPC_timeout destructor
 */
PUBLIC virtual inline NEEDS [IPC_timeout::owner, "receiver.h"]
IPC_timeout::~IPC_timeout()
{
  owner()->set_timeout (nullptr);	// reset owner's timeout field
}

PRIVATE inline NEEDS ["globals.h"]
Receiver *
IPC_timeout::owner()
{
  // We could have saved our context in our constructor, but computing
  // it this way is easier and saves space. We can do this as we know
  // that IPC_timeouts are always created on the kernel stack of the
  // owner context.

  return reinterpret_cast<Receiver *>(context_of (this));
}

/**
 * Timeout expiration callback function
 * \retval Reschedule::Yes if a reschedule is necessary
 * \retval Reschedule::No  if no reschedule is necessary
 */
PRIVATE
Reschedule
IPC_timeout::expired() override
{
  Receiver * const _owner = owner();

  Mword ipc_state = _owner->state() & Thread_ipc_mask;
  if (!ipc_state || (ipc_state & Thread_receive_in_progress))
    return Reschedule::No;

  _owner->state_add_dirty(Thread_ready | Thread_timeout);

  // Flag reschedule if owner's priority is higher than the current
  // thread's (own or timeslice-donated) priority.
  Sched_context *cur_ctx = current()->sched();
  return Sched_context::rq.current().deblock(_owner->sched(), cur_ctx, false);
}
