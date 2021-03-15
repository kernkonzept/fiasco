INTERFACE:

#include "member_offs.h"
#include <types.h>

#ifdef NO_INSTRUMENT
#undef NO_INSTRUMENT
#endif
#define NO_INSTRUMENT __attribute__((no_instrument_function))

class Context;


/**
 * A lock that implements priority inheritance.
 *
 * The lock uses a validity checker for doing an existence check before the lock
 * is actually acquired. With this mechanism the lock itself may disappear while
 * it is locked (see clear_no_switch_dirty() and switch_dirty()), even if it is
 * under contention. When the lock no longer exists, valid() returns false.
 *
 * The operations lock(), lock_dirty(), test(), test_and_set(), and
 * test_and_set_dirty() may return #Invalid if the lock does no longer exist.
 *
 * The validity checker is used while acquiring the lock to test if the lock
 * itself exists. We assume that a lock may disappear while we are blocked on
 * it.
 */
class Switch_lock
{
  MEMBER_OFFSET();

private:
  // Warning: This lock's member variables must not need a
  // constructor.  Switch_lock instances must assume
  // zero-initialization or be initialized using the initialize()
  // member function.
  // Reason: to avoid overwriting the lock in the thread-ctor
  Address _lock_owner;

public:
  /**
   * The result type of lock operations.
   */
  enum Status 
  {
    Not_locked, ///< The lock was formerly not acquired and -- we got it
    Locked,     ///< The lock was already acquired by ourselves
    Invalid     ///< The lock does not exist (is invalid)
  };

  /**
   * Stores the context of the lock for a later switch.
   * (see clear_no_switch_dirty(), switch_dirty())
   */
  struct Lock_context
  {
    Context *owner;
  };
};

#undef NO_INSTRUMENT
#define NO_INSTRUMENT 


IMPLEMENTATION:

#include <cassert>

#include "atomic.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "context.h"
#include "globals.h"
#include "processor.h"


//
// Switch_lock inlines
//

/**
 * Test if the lock is valid.
 *
 * \return true if the lock really exists, false if not.
 */
PUBLIC
inline
bool NO_INSTRUMENT
Switch_lock::valid() const
{ return (_lock_owner & 1) == 0; }

/**
 * Initialize Switch_lock.
 *
 * Call this function if you cannot guarantee that your Switch_lock instance is
 * allocated from zero-initialized memory.
 */
PUBLIC
inline
void NO_INSTRUMENT
Switch_lock::initialize()
{
  _lock_owner = 0;
}

/**
 * Lock owner.
 *
 * \return current owner of the lock. 0 if there is no owner.
 */
PUBLIC
inline NEEDS["lock_guard.h", "cpu_lock.h"]
Context * NO_INSTRUMENT
Switch_lock::lock_owner() const
{
  auto guard = lock_guard(cpu_lock);
  return (Context*)(_lock_owner & ~1UL);
}

/**
 * Is lock set?
 *
 * \retval #Locked      The lock is set.
 * \retval #Not_locked  The lock is not set.
 * \retval #Invalid     The lock does not exist (see valid()).
 */
PUBLIC
inline NEEDS["lock_guard.h", "cpu_lock.h"]
Switch_lock::Status NO_INSTRUMENT
Switch_lock::test() const
{
  auto guard = lock_guard(cpu_lock);
  Address o = access_once(&_lock_owner);
  if (EXPECT_FALSE(o & 1))
    return Invalid;
  return o ? Locked : Not_locked;
}

/**
 * Acquire the lock with priority inheritance.
 *
 * If the lock is occupied, lend the CPU to current lock owner until we are the
 * lock owner.
 *
 * \retval #Locked      The lock was already locked by the current context.
 * \retval #Not_locked  The current context got the lock (the usual case).
 * \retval #Invalid     The lock does not exist (see valid()).
 */
PUBLIC
Switch_lock::Status NO_INSTRUMENT
Switch_lock::lock()
{
  auto guard = lock_guard(cpu_lock);
  return lock_dirty();
}

PRIVATE
void NO_INSTRUMENT
Switch_lock::help(Context *curr, Context *owner, Address owner_id)
{
  auto s = curr->switch_exec_helping(owner, &_lock_owner, owner_id);
  if (s == Context::Switch::Failed)
    {
      Proc::preemption_point();
      if (curr->home_cpu() != current_cpu())
        curr->schedule();
    }
}

/**
 * \copydoc Switch_lock::lock()
 *
 * \pre caller holds cpu lock
 */
PUBLIC
inline NEEDS["context.h", "processor.h", Switch_lock::set_lock_owner]
Switch_lock::Status NO_INSTRUMENT
Switch_lock::lock_dirty()
{
  assert(cpu_lock.test());

  Mword o = access_once(&_lock_owner);
  if (EXPECT_FALSE(o & 1))
    return Invalid;

  Context *c = current();
  if (o == Address(c))
    return Locked;

  do
    {
      for (;;)
        {
          Mword o = access_once(&_lock_owner);
          if (o & 1)
            return Invalid;

          if (!o)
            break;

          help(c, (Context *)o, o);
        }
    }
  while (!set_lock_owner(c));
  Mem::mp_wmb();
  c->inc_lock_cnt();   // Do not lose this lock if current is deleted
  return Not_locked;
}


/**
 * \copydoc Switch_lock::lock()
 */
PUBLIC
inline NEEDS["globals.h"]
Switch_lock::Status NO_INSTRUMENT
Switch_lock::test_and_set()
{
  return lock();
}

/**
 * \copydoc Switch_lock::lock_dirty()
 */
PUBLIC
inline NEEDS["globals.h"]
Switch_lock::Status NO_INSTRUMENT
Switch_lock::test_and_set_dirty()
{
  return lock_dirty();
}

IMPLEMENTATION [!mp]:

PRIVATE inline
void NO_INSTRUMENT
Switch_lock::clear_lock_owner()
{
  _lock_owner &= 1;
}

/**
 * Set the lock owner.
 *
 * \pre must only be called on a free lock
 */
PRIVATE inline
bool NO_INSTRUMENT
Switch_lock::set_lock_owner(Context *o)
{
  _lock_owner = Address(o) | (_lock_owner & 1);
  return true;
}

PRIVATE static inline
void
Switch_lock::schedule(Context *curr)
{ curr->schedule(); }


IMPLEMENTATION [mp]:

PRIVATE inline
void NO_INSTRUMENT
Switch_lock::clear_lock_owner()
{
  atomic_mp_and(&_lock_owner, 1);
}

PRIVATE inline
bool NO_INSTRUMENT
Switch_lock::set_lock_owner(Context *o)
{
  bool have_no_locks = access_once(&o->_lock_cnt) < 1;

  if (have_no_locks)
    {
      assert (current_cpu() == o->home_cpu());
      while (EXPECT_FALSE(!o->_running_under_lock.try_dispatch()))
        ;
    }
  else
    assert (o->_running_under_lock);

  Mem::mp_wmb();

  if (EXPECT_FALSE(!mp_cas(&_lock_owner, Mword(0), Address(o))))
    {
      if (have_no_locks)
        {
          Mem::mp_wmb();
          o->_running_under_lock.reset();
        }
      return false;
    }

  return true;
}

PRIVATE static inline
void
Switch_lock::schedule(Context *curr)
{
  curr->schedule();
  /* we have to recheck the correct setting of `curr->_running_under_lock`
   * after schedule, because the home CPU of `curr` might have been set to
   * the current CPU meanwhile.
   * In this case and if `curr` has no locks anymore _running_under_lock
   * must be `false`. (Invariant: `curr->_running_under_lock` must be true
   * if `curr` executes on any CPU and `lock_cnt` is not zero, or if
   * `lock_cnt` is zero and `curr` is executing on a CPU other than the home
   * CPU of `curr`.)
   */
  if ((curr->home_cpu() == current_cpu())
      && curr->lock_cnt() == 0)
    curr->_running_under_lock.preempt();
}


IMPLEMENTATION:

/**
 * Clear the lock, however do not switch to a potential helper yet.
 *
 * This function is used when the lock must be cleared and the object
 * containing the lock will be deallocated atomically. We have to do the
 * switch later using switch_dirty(Lock_context).
 *
 * \return the context for a later switch_dirty()
 *
 * \pre caller holds cpu lock
 *
 * \post switch_dirty() must be called in the same atomical section
 */
PROTECTED
inline NEEDS[Switch_lock::clear_lock_owner, "context.h"]
Switch_lock::Lock_context NO_INSTRUMENT
Switch_lock::clear_no_switch_dirty()
{
  Mem::mp_wmb();
  Lock_context c;
  c.owner = lock_owner();
  clear_lock_owner();
  c.owner->dec_lock_cnt();
  return c;
}

/**
 * Do the switch part of clear() after a clear_no_switch_dirty().
 * This function does not touch the lock itself (may be called on
 * an invalid lock).
 *
 * \param c  the context returned by a former clear_no_switch_dirty().
 *
 * \pre must be called atomically with clear_no_switch_dirty(),
 *      under the same cpu lock
 */
PROTECTED
static inline NEEDS[Switch_lock::schedule]
void NO_INSTRUMENT
Switch_lock::switch_dirty(Lock_context const &c)
{
  assert (current() == c.owner);

  Context *curr = c.owner;
  Context *h = curr->helper();

  /*
   * If someone helped us by lending its time slice to us.
   * Just switch back to the helper without changing its helping state.
   */
  bool need_sched = false;

  if (h != curr)
    if (   EXPECT_FALSE(h->home_cpu() != current_cpu())
        || EXPECT_FALSE((long)curr->switch_exec_locked(h, Context::Ignore_Helping)))
      need_sched = true;

  if (!need_sched)
    need_sched = (   curr->lock_cnt() == 0
                  && curr->home_cpu() != current_cpu());

  if (EXPECT_FALSE(need_sched))
    schedule(curr);
}

/**
 * Free the lock.
 *
 * Return the CPU to helper if there is one, since it had to have a
 * higher priority to be able to help (priority may be its own, it
 * may run on a donated timeslice or round robin scheduling may have
 * selected a thread on the same priority level as me)
 */
PUBLIC
void NO_INSTRUMENT
Switch_lock::clear()
{
  auto guard = lock_guard(cpu_lock);

  switch_dirty(clear_no_switch_dirty());
}

PUBLIC
void NO_INSTRUMENT
Switch_lock::set(Status s)
{
  if (s == Not_locked)
    clear();
}

/**
 * Free the lock.
 *
 * Return the CPU to helper if there is one, since it had to have a
 * higher priority to be able to help (priority may be its own, it
 * may run on a donated timeslice or round robin scheduling may have
 * selected a thread on the same priority level as me).
 * If _lock_owner is 0, then this is a no op
 *
 * \pre caller holds cpu lock
 */
PUBLIC
inline
void NO_INSTRUMENT
Switch_lock::clear_dirty()
{
  assert(cpu_lock.test());

  switch_dirty(clear_no_switch_dirty());
}

PUBLIC inline
void NO_INSTRUMENT
Switch_lock::invalidate()
{
  auto guard = lock_guard(cpu_lock);
  atomic_mp_or(&_lock_owner, 1);
}

PUBLIC
void NO_INSTRUMENT
Switch_lock::wait_free()
{
  auto guard = lock_guard(cpu_lock);
  Context *c = current();

  assert (!valid());

  if ((_lock_owner & ~1UL) == (Address)c)
    {
      clear_lock_owner();
      c->dec_lock_cnt();
      return;
    }

  for(;;)
    {
      assert(cpu_lock.test());

      Address _owner = access_once(&_lock_owner);
      Context *owner = (Context *)(_owner & ~1UL);
      if (!owner)
        break;

      help(c, owner, _owner);
    }
}
