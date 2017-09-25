INTERFACE:

#include "member_offs.h"
#include "context.h"		// Switch_hint
#include "panic.h"
#include "switch_lock.h"

enum Switch_hint
{
  SWITCH_ACTIVATE_HIGHER = 0,	// Activate thread with higher priority
  SWITCH_ACTIVATE_LOCKEE,	// Activate thread that was locked
};


/** Thread lock.
    This lock uses the basic priority-inheritance mechanism (Switch_lock)
    and extends it in two ways: First, it has a hinting mechanism that
    allows a locker to specify whether the clear() operation should
    switch to the thread that was locked.  Second, it maintains the current
    locker for Context; this locker automatically gets CPU time allocated
    to the locked thread (Context's ``donatee''); 
    Context::switch_exec() uses that hint.
    To make clear, which stuff in the TCB the lock not protects:
    -the thread state
    -queues
    -the raw kernelstack
    The rest is protected with this lock, this includes the
    kernelstackpointer (kernel_sp).
 */
class Thread_lock : private Switch_lock
{
  MEMBER_OFFSET();

private:
  typedef Switch_lock Lock;

public:
  using Lock::Invalid;
  using Lock::Locked;
  using Lock::Not_locked;

  using Lock::Lock_context;
  
  using Lock::test;
  using Lock::lock_owner;
  using Lock::Status;

  /*
   * Can use the Switch_lock version, because we assume the context
   * is invalid during switch_dirty and we do not need to consider it
   * for scheduling any more.
   */
  using Lock::clear_no_switch_dirty;
  using Lock::switch_dirty;

private:
  Switch_hint _switch_hint;
};

IMPLEMENTATION:

#include "globals.h"
#include "lock_guard.h"
#include "cpu_lock.h"
#include "thread_state.h"
#include "cpu.h"

/** Context this thread lock belongs to.
    @return context locked by this thread lock
 */
PRIVATE inline
Context *
Thread_lock::context() const
{
  // We could have saved our context in our constructor, but computing
  // it this way is easier and saves space.  We can do this as we know
  // that thread_locks are always embedded in their corresponding
  // context.
  return context_of (this);
}

/** Set switch hint.
    @param hint a hint to the clear() function
 */
PUBLIC inline
void
Thread_lock::set_switch_hint (Switch_hint const hint)
{
  _switch_hint = hint;
}

/** Lock a context.
    @return true if we owned the lock already.  false otherwise.
 */
PUBLIC
Thread_lock::Status
Thread_lock::test_and_set()
{
  auto guard = lock_guard(cpu_lock);
  return test_and_set_dirty();
}

/** Lock a context.
    @return true if we owned the lock already.  false otherwise.
    @pre caller holds cpu lock
 */
PRIVATE inline NEEDS["switch_lock.h","cpu_lock.h","globals.h", Thread_lock::context]
Thread_lock::Status
Thread_lock::test_and_set_dirty()
{
  assert(cpu_lock.test());

  switch (Status r = Lock::test_and_set ())
    {
    case Invalid:
    case Locked:
      return r;
    case Not_locked:
      break;
    }

  context()->set_donatee (current()); // current get time of context

  set_switch_hint (SWITCH_ACTIVATE_HIGHER);

  return Not_locked;
}


/** Lock a thread.
    If the lock is occupied, enqueue in list of helpers and lend CPU
    to current lock owner until we are the lock owner.
 */
PUBLIC inline NEEDS["globals.h"]
Thread_lock::Status
Thread_lock::lock()
{
  return test_and_set();
}

/** Lock a thread.
    If the lock is occupied, enqueue in list of helpers and lend CPU 
    to current lock owner until we are the lock owner.
    @pre caller holds cpu lock
 */
PUBLIC inline NEEDS[Thread_lock::test_and_set_dirty, "globals.h"]
Thread_lock::Status
Thread_lock::lock_dirty()
{
  // removed assertion, because we do lazy locking
  return test_and_set_dirty();
}

/** Free the lock.
    First return the CPU to helper or next lock owner, whoever has the higher
    priority, given that thread's priority is higher that ours.
    Finally, switch to locked thread if that thread has a higher priority,
    and/or the switch hint says we should.
 */
PUBLIC
void
Thread_lock::clear()
{
  Switch_hint hint = _switch_hint;	// Save hint before unlocking

  auto guard = lock_guard(cpu_lock);

  // Passing on the thread lock implies both passing _switch_lock
  // and setting context()'s donatee to the new owner.  This must be
  // accomplished atomically.  There are two cases to consider:
  // 1. _switch_lock.clear() does not switch to the new owner.
  // 2. _switch_lock.clear() switches to the new owner.
  // To cope with both cases, we set the owner both here (*) and in
  // Thread_lock::test_and_set().  The assignment that is executed
  // first is always atomic with _switch_lock.clear().  The
  // assignment that comes second is redundant.
  // Note: Our assignment (*) might occur at a time when there is no
  // lock owner or even when the context() has been killed.
  // Fortunately, it works anyway because contexts live in
  // type-stable memory.
  Lock::clear();
  context()->set_donatee(lock_owner());

  // We had locked ourselves, remain current
  if (context() == current())
    return;

  // Unlocked thread not ready, remain current
  if (!(context()->state() & Thread_ready))
    return;

  // Switch to lockee's execution context if the switch hint says so
  if (hint == SWITCH_ACTIVATE_LOCKEE)
    {
      check(current()->switch_exec_locked (context(), Context::Not_Helping)
            == Context::Switch::Ok);
      return;
    }

  // Switch to lockee's execution context and timeslice if its priority
  // is higher than the current priority
  if (Sched_context::rq.current().deblock(context()->sched(), current()->sched(), true))
    current()->switch_to_locked(context());
}

PUBLIC inline
void
Thread_lock::set(Status s)
{
  if (s == Not_locked)
    clear();
}

/** Free the lock.
    First return the CPU to helper or next lock owner, whoever has the higher 
    priority, given that thread's priority is higher that ours.
    Finally, switch to locked thread if that thread has a higher priority,
    and/or the switch hint says we should.
    Assumes caller hold cpu lock
 */
PUBLIC
void
Thread_lock::clear_dirty()
{
  assert(cpu_lock.test());

  Switch_hint hint = _switch_hint;

  if(test())
    {
      Lock::clear_dirty();
      context()->set_donatee(lock_owner());
    }
  else
    {
      //      assert(!context()->donatee());
    }

  assert(cpu_lock.test());

  // We had locked ourselves, remain current
  if (context() == current())
    return;

  // Unlocked thread not ready, remain current
  if (!(context()->state() & Thread_ready))
    return;

  // Switch to lockee's execution context if the switch hint says so
  if (hint == SWITCH_ACTIVATE_LOCKEE)
    {
      check(current()->switch_exec_locked(context(), Context::Not_Helping)
            == Context::Switch::Ok);
      return;
    }

  // Switch to lockee's execution context and timeslice if its priority
  // is higher than the current priority
  if (Sched_context::rq.current().deblock(context()->sched(), current()->sched(), true))
    current()->switch_to_locked(context());
}

/** Free the lock.
    First return the CPU to helper or next lock owner, whoever has the higher 
    priority, given that thread's priority is higher that ours.
    Finally, switch to locked thread if that thread has a higher priority,
    and/or the switch hint says we should.
    Assumes caller hold cpu lock
 */
PUBLIC inline
void
Thread_lock::clear_dirty_dont_switch()
{
  assert(cpu_lock.test());

  if(EXPECT_TRUE(!test()))
    return;

  Lock::clear_dirty();
  context()->set_donatee(lock_owner());

  assert(cpu_lock.test());
}



