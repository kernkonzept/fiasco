IMPLEMENTATION [prio_inherit]:

#include "atomic.h"
#include "kmem_slab.h"
#include "kobject_dbg.h"
#include "kobject_helper.h"
#include "kobject_rpc.h"
#include "task.h"
#include "thread_state.h"
#include "timer.h"
#include "warn.h"

JDB_DEFINE_TYPENAME(Pi_mutex, "PI Mutex");

class Pi_mutex;

/**
 * Represents a thread waiting on a mutex to acquire it.
 *
 * Allocated on the stack of the waiting thread, then enqueued in the mutex's
 * waiter list.
 */
struct Pi_mutex_waiter : public Prio_list_elem
{
  Pi_mutex_waiter(Thread *thread, Pi_mutex *mutex)
  : thread(thread), mutex(mutex)
  {}

  Pi_mutex_waiter(Pi_mutex_waiter const &) = delete;
  Pi_mutex_waiter &operator=(Pi_mutex_waiter const &) = delete;

  Thread *thread;  // Thread that is waiting to acquire the mutex.
  Pi_mutex *mutex; // Mutex the thread is waiting for.
};

/**
 * Priority inheritance-aware mutex.
 *
 * Needs to inherit from Prio_list_elem, since it is enqueued in
 * pi_owned_mutexes() prio list of the owner thread.
 */
class Pi_mutex final : public Kobject_h<Pi_mutex, Kobject>,
                       public Ref_cnt_obj,
                       public Pi_mutex_iface
{
public:
  enum class Op : Mword
  {
    Lock = 0x0,
    Unlock = 0x1,
  };

  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                     Utcb const *in, Utcb *)
  {
    L4_msg_tag tag = f->tag();

    if (!Ko::check_basics(&tag, L4_msg_tag::Label_pi_mutex))
      return tag;

    switch (Op{in->values[0]})
      {
      case Op::Lock: return sys_lock(rights, f, in);
      case Op::Unlock: return sys_unlock(rights, f, in);
      default: return commit_result(-L4_err::ENosys);
      }
  }

private:
  struct Pi_chain_walk_state
  {
    Pi_chain_walk_state(Thread *waiter, Pi_mutex *mutex)
    : cur_waiter(waiter),
      cur_owner(mutex->_owner),
      cur_mutex(mutex),
      depth(0)
    {}

    Thread *cur_waiter;
    Thread *cur_owner;
    Pi_mutex *cur_mutex;
    unsigned depth;

    /**
     * \param  waiter  The waiter thread that initiated the chain walk.
     *
     * \return Whether a deadlock condition is detected.
     */
    bool detect_deadlock(Thread *waiter)
    {
      if (depth >= Max_prio_chain_depth) [[unlikely]]
        {
          // Reached maximum priority chain depth, assume deadlock.
          WARNX(Info, "pi_mutex: Reached maximum priority chain depth, assume deadlock.");
          return true;
        }

      // NOTE: Due to the prio boosting early exit condition, we actually are
      //       never caught in a loop for wait=1, since once the loop closes the
      //       `from <= to` no propagate early exit hits.
      //       However for wait=0, that is not true, since there the condition
      //       is `from > to`. But I added another early exit, if recalculated
      //       mutex prio is unchanged, thereby also breaking the deadlock loop.
      if (waiter == cur_owner) [[unlikely]]
        {
          // Thread that started waiting on mutex A is also owner of another
          // mutex B in the PI chain of mutex A, deadlock detected.
          WARNX(Info, "pi_mutex: Waiter equal owner, deadlock detected.");
          return true;
        }

      return false;
    }

    /**
     * Follow chain.
     *
     * \pre `cur_owner->pi_blocked_on() != nullptr`
     */
    void follow_blocked()
    {
      cur_mutex = cur_owner->pi_blocked_on()->mutex;
      cur_waiter = cur_owner;
      cur_owner = cur_mutex->_owner;
      depth++;
    }
  };

  /// Indicates that the mutex is in slow path, there might be (pending) waiters.
  /// The kernel clears this flag when it transitions the mutex into fast path,
  /// i.e. when a thread releases the mutex and its waiters queue is empty.
  static constexpr Mword Pi_waiters_flag = 1;
  static constexpr Mword Pi_owner_mask = ~((1UL << L4_obj_ref::Cap_shift) - 1);

  /// Placeholder until we add a tid debug parameter to sys_lock/unlock(), that
  /// would make it transparent to user-space which thread currently owns the
  /// mutex.
  static constexpr Mword Some_owner_tid = Pi_owner_mask;

  /// Maximum allowed PI chain depth, exceeding this will be interpreted as a
  /// deadlock condition.
  static constexpr unsigned Max_prio_chain_depth = 128;
  /// No early exits when walking PI chain, to detect deadlocks.
  static constexpr bool Detect_deadlocks = true;

  /**
   * The pi_chain_lock protects the PI waiter chain data structures.
   * Since a Pi_mutex cannot be shared between tasks, it can be a per-task lock
   * (instead of a global lock). To ensure that the correct pi_chain_lock is
   * used, we check at the start of sys_(un)lock() that the task the PI mutex is
   * bound to is the same as the task of the calling thread.
   */
  Spin_lock<> &pi_chain_lock()
  {
    return _space->pi_chain_lock;
  }

  /// Allocation quota.
  Ram_quota *_quota;

  /**
   * Task this Pi_mutex belongs to, assigned at creation, cannot be changed
   * afterwards. The kernel uses the space to lookup the ku_status field in
   * kernel-user memory and fast path owner tids.
   * The PI mutex keeps a counted reference to the task, which it releases in
   * its destructor, i.e. after the mutex object was destroyed and all
   * owner/waiter threads released there hold on the mutex.
   * NOTE: If the task grants the PI mutex object to a different task, the
   * counted reference results in a circular dependency preventing
   * deletion of the task and this PI mutex object (same issue as with threads).
   */
  Ref_ptr<Space> _space;

  /**
   * Kernel address of the mutex status field in kernel-user memory.
   * The liveness of this is guaranteed by holding a counted reference to the
   * task assigned to the Pi_mutex at creation.
   * The kernel-user memory is only freed in the destructor of the task.
   *
   * The kernel does not use _ku_status for internal synchronization. It
   * instead uses the pi_chain_lock Spin_lock to protect the state of the
   * Pi_mutex. That already ensures that the memory ordering semantics of the
   * mutex expected by user-space are upheld. However, there are two edge cases,
   * where the correct semantics of the CAS on _ku_status are important:
   *
   * - Acquire from fast path release in user-space (cas_acquire in sys_lock()).
   * - Release to user-space, back to fast path (cas_release in sys_unlock()).
   *
   * OPTIMIZE: Under contention, a CAS loop might take long, depending on its
   *           implementation. So maybe the kernel should use a
   *           CAS-implementation with a bounded loop when accessing _ku_status.
   */
  Mword *_ku_status;

  /// List of threads waiting to acquire this mutex.
  Prio_list/*<Pi_mutex_waiter>*/ _waiters;

  /**
   * Protects state of Pi_mutex.
   * NOTE: Unused for now, we rely on the pi_chain_lock, as with our current
   * algorithm, we need to have a consistent view of all Thread::_pi_* members,
   * Pi_mutex::_owner and Pi_mutex::_waiters.
   * Spin_lock<> _lock;
   *
   * We don't need a counted reference to owner, because when a  thread is
   * killed it releases all mutexes it owns (pi_owned_mutexes()).
   */
  Thread *_owner = nullptr;

  /// Flag that indicates whether last owner died.
  bool _owner_died = false;
};

/**
 * Timeout used when waiting on mutex.
 *
 * Allocated on the stack of the waiting thread.
 */
class Pi_wait_timeout : public Timeout
{
public:
  ~Pi_wait_timeout()
  {
    waiter()->set_timeout(nullptr); // reset owner's timeout field
  }

private:
  Receiver *waiter()
  {
    // We could have saved our context in our constructor, but computing
    // it this way is easier and saves space. We can do this as we know
    // that `Pi_wait_timeout`s are always created on the kernel stack of
    // the owner context.
    return reinterpret_cast<Receiver *>(context_of(this));
  }

  Reschedule expired() override
  {
    Receiver *const _waiter = waiter();

    if (!(_waiter->state() & Thread_pi_mutex_wait))
      // Wait already finished.
      return Reschedule::No;

    _waiter->state_add_dirty(Thread_ready | Thread_timeout);

    // Flag reschedule if owner's priority is higher than the current
    // thread's (own or timeslice-donated) priority.
    Sched_context *cur_ctx = current()->sched();
    return Sched_context::rq.current().deblock(_waiter->sched(), cur_ctx, false);
  }
};

/**
 * Enqueue thread into waiter list of this mutex.
 *
 * \pre pi_chain_lock must be held.
 */
PRIVATE inline
void
Pi_mutex::enqueue_waiter(Pi_mutex_waiter *waiter)
{
  assert(waiter->thread->pi_blocked_on() == nullptr);

  _waiters.insert(waiter, waiter->thread->pi_effective_prio());
  waiter->thread->set_pi_blocked_on(waiter);
}

/**
 * Dequeue thread from waiter list of this mutex.
 *
 * \pre pi_chain_lock must be held.
 */
PRIVATE inline
void
Pi_mutex::dequeue_waiter(Pi_mutex_waiter *waiter)
{
  assert(waiter->thread->pi_blocked_on() == waiter);
  assert(waiter->thread->pi_blocked_on()->mutex == this);

  waiter->thread->set_pi_blocked_on(nullptr);
  _waiters.dequeue(waiter);
}

/**
 * Set thread as owner and take counted reference to mutex.
 *
 * \param owner       Thread to set as owner.
 * \param check_dead  Check if owner is dying or dead (killed or not yet
 *                    started). Required for owner that acquires the mutex
 *                    via fast path.
 *
 * \retval true   Success.
 * \retval false  Failed to set owner, it is dying or dead.
 *
 * \pre _owner == nullptr
 * \pre pi_chain_lock must be held.
 */
PRIVATE inline
bool
Pi_mutex::set_owner(Thread *owner, bool check_dead)
{
  assert(_owner == nullptr);

  auto g = lock_guard<No_cpu_lock_policy>(owner->pi_owned_mutexes().lock());
  // Since Thread takes pi_owned_mutexes().lock() at least once in
  // Thread::do_kill(), we are guaranteed to observe the Thread_dying state
  // flag.

  // Prevent dying or dead (killed or not yet started) thread from becoming
  // mutex owner. This solves multiple issues:
  //   - Dying thread cannot become owner of additional mutexes via fast path,
  //     so Context::release_owned_pi_mutexes() is guaranteed to eventually
  //     release all owned mutexes.
  //   - When a thread that was never started is destroyed, it does not go
  //     through Thread::do_kill(), i.e. will not execute
  //     Context::release_owned_pi_mutexes().
  //     In addition, it makes no sense for a never started thread to become
  //     fast path owner, this can only happen if user-space illegally tampers
  //     with ku_status.
  if (check_dead && (owner->state() & (Thread_dying | Thread_dead)))
    return false;

  inc_ref();
  _owner = owner;
  owner->pi_owned_mutexes().insert(this, prio());
  return true;
}

/**
 * Reset thread as owner and returns the counted reference to mutex.
 *
 * \return Counted reference the owner held of the mutex.
 *
 * \pre _owner != nullptr
 * \pre pi_chain_lock must be held.
 */
PRIVATE inline
Ref_ptr<Pi_mutex>
Pi_mutex::reset_owner()
{
  assert(_owner != nullptr);

  auto g = lock_guard<No_cpu_lock_policy>(_owner->pi_owned_mutexes().lock());
  _owner->pi_owned_mutexes().dequeue(this);
  Ref_ptr<Pi_mutex> ref = Ref_ptr<Pi_mutex>::from_counted_ref(this);
  _owner = nullptr;
  return ref;
}

/**
 * Boost priority of thread after it acquired this mutex.
 *
 * \param thread   Thread that acquired the mutex.
 *
 * \return  Whether thread needs PI reschedule.
 *
 * \pre pi_chain_lock must be held.
 */
PRIVATE
bool
Pi_mutex::adjust_prio_on_acquire(Thread *thread)
{
  // Since we know that thread does not block on a mutex at this time, no need
  // to propagate any outgoing priority.
  assert(thread->pi_blocked_on() == nullptr);

  // Compare priority of acquired mutex with the effective priority of the
  // thread. If priority of mutex is larger, boost the threads priority.
  if (prio() > thread->pi_effective_prio())
    {
      thread->set_pi_effective_prio(prio());
      return true; // need reschedule
    }

  return false;
}

/**
 * Reduce priority of thread after it released this mutex.
 *
 * \param thread   Thread that released the mutex.
 *
 * \return  Whether thread needs PI reschedule.
 *
 * \pre pi_chain_lock must be held.
 *
 * \note Does not access _owner field of the mutex.
 */
PRIVATE
bool
Pi_mutex::adjust_prio_on_release(Thread *thread)
{
  // Since we know that thread does not block on a mutex at this time, no need
  // to propagate any outgoing priority.
  assert(thread->pi_blocked_on() == nullptr);

  // Compare priority of released mutex with the effective priority of the
  // thread. If priority of released mutex is equal, it might have been the
  // maximum that dominated the threads effective priority. So we need to
  // calculate the new max priority from all mutexes the thread still owns.
  if (prio() == thread->pi_effective_prio())
    {
      auto new_prio = thread->pi_regular_prio();

      // Get highest prio from all mutexes the thread owns.
      auto *highest_prio = thread->pi_owned_mutexes().first();
      if (highest_prio && highest_prio->prio() > new_prio)
        new_prio = highest_prio->prio();

      if (new_prio != thread->pi_effective_prio())
        {
          thread->set_pi_effective_prio(new_prio);
          return true; // need reschedule
        }
    }

  return false;
}

/**
 * Adjust priorities in PI chain after thread starts to wait on mutex.
 *
 * \param waiter    Waiter thread.
 * \param deadlock  Set to `true` if deadlock is detected, otherwise not
 *                  accessed.
 *
 * \return  Thread that needs PI reschedule, if any.
 *
 * \pre pi_chain_lock must be held.
 * \pre This mutex must be locked, i.e. _owner must be set.
 * \pre Waiter must already be enqueued with the correct priority in the
        _waiters list of this mutex.
 *
 * \note Both _owner and _owner->pi_owned_mutexes() to be up-to-date.
 */
PRIVATE
Thread *
Pi_mutex::adjust_prio_chain_on_start_wait(Thread *waiter, bool *deadlock)
{
  Pi_chain_walk_state s(waiter, this);
  // The PI chain walk is implemented in a static function to avoid accidental
  // usage of `this`.
  return Pi_mutex::_adjust_prio_chain_on_wait(s, waiter, true, deadlock);
}

/**
 * Adjust priorities in PI chain after thread stops to wait on mutex.
 *
 * \param waiter    Waiter thread.
 * \param deadlock  Set to `true` if deadlock is detected, otherwise not
 *                  accessed.
 *
 * \return  Thread that needs PI reschedule, if any.
 *
 * \pre pi_chain_lock must be held.
 * \pre This mutex must be locked, i.e. _owner must be set.
 * \pre Waiter must already be dequeued.
 *
 * \note Both _owner and _owner->pi_owned_mutexes() to be up-to-date.
 */
PRIVATE
Thread *
Pi_mutex::adjust_prio_chain_on_stop_wait(Thread *waiter, bool *deadlock)
{
  Pi_chain_walk_state s(waiter, this);
  // The PI chain walk is implemented in a static function to avoid accidental
  // usage of `this`.
  return Pi_mutex::_adjust_prio_chain_on_wait(s, waiter, false, deadlock);
}

PRIVATE static inline
Thread *
Pi_mutex::_adjust_prio_chain_on_wait(Pi_chain_walk_state &s, Thread *waiter,
                                     bool wait, bool *deadlock)
{
  auto no_propagate = [wait](unsigned short from, unsigned short to)
    {
      if (wait)
        // On start wait do no propagate if `from` priority is lower or equal
        // than `to` priority.
        return from <= to;
      else
        // On stop wait do no propagate if `from` priority is greater than `to`
        // priority.
        return from > to;
    };

  for (;;)
    {
      if (s.cur_mutex->_owner_died) [[unlikely]]
        return nullptr;

      if (s.detect_deadlock(waiter)) [[unlikely]]
        {
          *deadlock = true;
          return nullptr;
        }

      assert(s.cur_owner != nullptr);
      assert(s.cur_waiter != s.cur_owner);
      // TODO: Actually we would like to check that Mutex is in list of owner,
      //       not just in any list.
      assert(s.cur_mutex->in_list());

      // Waiter was already added/removed from s.cur_mutex->waiters, but
      // the update of s.cur_mutex->prio() only happens now.

      // 1. Compare effective priority of waiter with priority of mutex.
      if (no_propagate(s.cur_waiter->pi_effective_prio(), s.cur_mutex->prio()))
        // If ... do nothing.
        return finish_walk_detect_deadlock(waiter, s, deadlock);

      // OPTIMIZE: for wait=true this could be optimized as `s.cur_waiter->pi_effective_prio()`.
      // Recalculate new mutex prio.
      unsigned short new_mutex_prio = 0;
      if (auto *top_waiter = s.cur_mutex->_waiters.first())
        new_mutex_prio = top_waiter->prio();

      // Recalculated mutex prio equal to its previous prio, no need to
      // propagate further.
      // For wait=false this early exit breaks an endless deadlock loop.
      // For wait=true this is already implicitly contained in no_propagate(),
      // so removing this early exit would do the (useless) dequeue/insert below
      // and then take the no_propagate early exit.
      if (new_mutex_prio == s.cur_mutex->prio())
        return finish_walk_detect_deadlock(waiter, s, deadlock);

      // Otherwise adjust mutex priority and propagate new priority to owner.
      {
        auto g = lock_guard<No_cpu_lock_policy>(s.cur_owner->pi_owned_mutexes().lock());
        s.cur_owner->pi_owned_mutexes().dequeue(s.cur_mutex);
        s.cur_owner->pi_owned_mutexes().insert(s.cur_mutex, new_mutex_prio);
      }

      // 2. Compare new mutex prio with effective priority of owner.
      if (no_propagate(new_mutex_prio, s.cur_owner->pi_effective_prio()))
        // If ... do no propagation.
        return finish_walk_detect_deadlock(waiter, s, deadlock);

      // Otherwise adjust owner priority.
      // OPTIMIZE: for wait=true this could be optimized as `new_mutex_prio`.
      auto new_owner_prio = max(new_mutex_prio, s.cur_owner->pi_regular_prio());
      s.cur_owner->set_pi_effective_prio(new_owner_prio);

      // 3. Is owner blocked on mutex?
      if (!s.cur_owner->pi_blocked_on())
        {
          // Non-blocked owner whose priority got updated needs to be rescheduled.
          return s.cur_owner; // need reschedule
        }

      // Note: Blocked threads should never be in the ready queue, or at least
      // remove themselves after calling adjust_prio_chain_on_start_wait(). So
      // once they get unblocked eventually, they will be inserted into the
      // ready queue with their new effective priority. No need for us to force
      // a reschedule here.

      // Update position of blocked owner in waiter queue of mutex it waits for.
      auto &mutex_waiters = s.cur_owner->pi_blocked_on()->mutex->_waiters;
      mutex_waiters.dequeue(s.cur_owner->pi_blocked_on());
      mutex_waiters.insert(s.cur_owner->pi_blocked_on(),
                           s.cur_owner->pi_effective_prio());

      // Follow chain and propagate new priority.
      s.follow_blocked();
    }
}

/**
 * Walk the entire PI chain to detect deadlocks eagerly.
 *
 * Mostly relevant for errorcheck mutexes.
 *
 * The early exits based on no_propagate in _adjust_prio_chain_on_wait() hand
 * over to finish_walk_detect_deadlock().
 *
 * \return  nullptr, for ergonomic usage in _adjust_prio_chain_on_wait().
 */
PRIVATE static
Thread *
Pi_mutex::finish_walk_detect_deadlock(Thread *waiter,
                                      Pi_chain_walk_state &state,
                                      bool *deadlock)
{
  // The eager deadlock detection is optional, because even under a deadlock
  // condition the PI chain walk logic will NOT end up looping until it hits the
  // Max_prio_chain_depth, due to the early exits. So there is no danger for
  // system degradation in it.
  if (!Detect_deadlocks)
    return nullptr;

  for (;;)
    {
      if (!state.cur_owner->pi_blocked_on())
        break;

      state.follow_blocked();

      if (state.detect_deadlock(waiter)) [[unlikely]]
        {
          *deadlock = true;
          break;
        }
    }

  return nullptr;
}

/**
 * Boost priority of thread after it was announced as the fast path owner.
 *
 * \param fast_path_owner  Thread that got assigned as mutex owner.
 * \param deadlock         Set to `true` if deadlock is detected, otherwise
 *                         not accessed.
 *
 * \return  Thread that needs PI reschedule, if any.
 *
 * \pre pi_chain_lock must be held.
 */
PRIVATE
Thread *
Pi_mutex::adjust_prio_on_fast_path_acquire(Thread *fast_path_owner,
                                           bool *deadlock)
{
  // Compare priority of acquired mutex with the effective priority of the
  // fast path owner thread. If priority of mutex is larger, boost the threads
  // priority.
  if (prio() > fast_path_owner->pi_effective_prio())
    {
      fast_path_owner->set_pi_effective_prio(prio());

      // Fast path owners might be blocked on other mutex.
      if (fast_path_owner->pi_blocked_on() == nullptr) [[unlikely]]
        return fast_path_owner; // need reschedule

      // Requeue fast path waiter.
      Pi_mutex *blocked_on_mutex = fast_path_owner->pi_blocked_on()->mutex;
      blocked_on_mutex->_waiters.dequeue(fast_path_owner->pi_blocked_on());
      blocked_on_mutex->_waiters.insert(fast_path_owner->pi_blocked_on(),
                                        fast_path_owner->pi_effective_prio());

      // Since the prio of the fast path owner just got boosted, we need to
      // propagate it.
      return blocked_on_mutex->adjust_prio_chain_on_start_wait(fast_path_owner,
                                                               deadlock);
    }

  return nullptr;
}

PRIVATE static
bool
Pi_mutex::setup_wait_timer(Thread *waiter, L4_timeout timeout, Utcb const *utcb,
                           Timeout *timer)
{
  if (timeout.is_never())
    return true;

  if (timeout.is_zero()) [[unlikely]]
    return false;

  Unsigned64 clock = Timer::system_clock();
  Unsigned64 tval = timeout.microsecs(clock, utcb);

  if ((tval <= clock)) [[unlikely]]
    // timeout already hit
    return false;

  waiter->set_timeout(timer, tval);
  return true;
}

/**
 * Establish fast path owner of mutex.
 *
 * \retval true   On success.
 * \retval false  Fast path owner tid did not resolve to a Thread object.
 *
 * \pre pi_chain_lock must be held.
 */
PRIVATE inline
bool
Pi_mutex::establish_fast_path_owner(Thread *self, Mword fast_path_owner_tid,
                                    Thread **fast_prio_updated,
                                    unsigned short *fast_prio, bool *deadlock)
{
  // PI mutex can only be used from the space it was bound to during creation.
  if (self->space() != _space.get()) [[unlikely]]
    return false;

  // Now that we ensured that fast path owner needs to go through kernel, i.e.
  // will block at the lock we are holding, lookup via its cap.
  L4_fpage::Rights owner_rights = L4_fpage::Rights(0);
  Thread *fast_path_owner = cxx::dyn_cast<Thread *>(self->space()->lookup_local(
    L4_obj_ref(fast_path_owner_tid).cap(), &owner_rights));

  if (!fast_path_owner) [[unlikely]]
    return false;

  if (!set_owner(fast_path_owner, /* check_dead = */ true)) [[unlikely]]
    {
      // Fast path owner was killed before it could release the lock.
      _owner_died = true;
      return true;
    }

  *fast_prio_updated = adjust_prio_on_fast_path_acquire(fast_path_owner, deadlock);
  if (*fast_prio_updated)
    *fast_prio = (*fast_prio_updated)->pi_effective_prio();

  return true;
}

PRIVATE inline
bool
Pi_mutex::prepare_wait_on_pi_mutex(Thread *self, Pi_mutex_waiter &self_waiter,
                                   Thread **chain_prio_updated,
                                   unsigned short *chain_prio, bool *deadlock)
{
  enqueue_waiter(&self_waiter);
  *chain_prio_updated = adjust_prio_chain_on_start_wait(self, deadlock);
  if (*deadlock) [[unlikely]]
    {
      // If deadlock is detected, abort wait and return error.
      dequeue_waiter(&self_waiter);
      adjust_prio_chain_on_stop_wait(self, deadlock);
      *chain_prio_updated = nullptr;
      return false;
    }
  else
    {
      if (*chain_prio_updated)
        *chain_prio = (*chain_prio_updated)->pi_effective_prio();
      // OPTIMIZE: Probably not necessary to do this under lock.
      self->state_change_dirty(~Thread_ready, Thread_pi_mutex_wait);
      return true;
    }
}

PRIVATE inline
L4_msg_tag
Pi_mutex::wait_on_pi_mutex(Thread *self, Pi_mutex_waiter &self_waiter,
                           Utcb const *utcb)
{
  // We might have switched to a higher-priority thread between starting the
  // wait on the mutex and now. So it might very well be that we got the mutex.
  if (!(self->state() & Thread_pi_mutex_wait))
    {
      // Timeout might have hit, but we are mutex owner now, so ignore it.
      self->state_del_dirty(Thread_timeout | Thread_cancel);
      // Our wait has ended, we acquired the mutex.
      return commit_result(0);
    }

  // Wait until we acquire the mutex or timeout.
  for (;;)
    {
      self->state_del_dirty(Thread_ready);
      self->schedule();

      auto new_state = self->state();

      if (!(new_state & Thread_pi_mutex_wait))
        {
          // Timeout might have hit, but we are mutex owner now, so ignore it.
          self->state_del_dirty(Thread_timeout | Thread_cancel);
          return commit_result(0);
        }

      if (!(new_state & (Thread_timeout | Thread_cancel))) [[unlikely]]
        // Spurious wakeup, continue wait.
        continue;

      Thread *timeout_prio_updated = nullptr;
      // Note: We read the prio under lock.
      unsigned short timeout_prio;
      {
        // Check under lock whether we own or wait on the mutex.
        auto guard = lock_guard<No_cpu_lock_policy>(pi_chain_lock());

        // Check if we acquired the mutex, but the xcpu_pi_update_prio()
        // hasn't arrived yet.
        if (!self_waiter.in_list())
          {
            assert(_owner == self);
            // Timeout hit, but we are mutex owner already, so ignore it.
            // Continue wait until xcpu_pi_update_prio() notifies us.
            self->state_del_dirty(Thread_timeout | Thread_cancel);
            continue;
          }

        // First dequeue waiter.
        dequeue_waiter(&self_waiter);
        bool deadlock = false;
        timeout_prio_updated = adjust_prio_chain_on_stop_wait(self, &deadlock);
        if (timeout_prio_updated)
          timeout_prio = timeout_prio_updated->pi_effective_prio();

        self->state_del_dirty(Thread_pi_mutex_wait);
      }

      if (timeout_prio_updated
          && timeout_prio_updated->xcpu_pi_update_prio(timeout_prio) == Reschedule::Yes)
        self->switch_to_locked(timeout_prio_updated);

      return commit_error(utcb, new_state & Thread_timeout
                                  ? L4_error::R_timeout
                                  : L4_error::R_canceled);
    }
}

/*
 * L4-IFACE: kernel-pi_mutex.pi_mutex-lock
 * PROTOCOL: L4_PROTO_PI_MUTEX
 */
PRIVATE inline
L4_msg_tag
Pi_mutex::sys_lock(L4_fpage::Rights, Syscall_frame *f, Utcb const *utcb)
{
  if (f->tag().words() != 1) [[unlikely]]
    return commit_result(-L4_err::EInval);

  Thread *self = current_thread();

  // PI mutex can only be used from the space it was bound to during creation.
  if (self->space() != _space.get()) [[unlikely]]
    return commit_result(-L4_err::EPerm);

  // OPTIMIZE: Since we hold a reference to Pi_mutex, we could handle the
  // -L4_err::EAgain cases below directly in the kernel, by adding a preemption
  // point and then retrying the operation, instead of returning the error to
  // user-space (which will then call sys_lock() again).

  // Keep PI mutex alive, in case it gets deleted while we wait (includes
  // scheduling away to higher priority threads even before entering
  // wait_on_pi_mutex()).
  Ref_ptr<Pi_mutex> _pi_mutex(this);

  Pi_wait_timeout wait_timeout;
  Pi_mutex_waiter self_waiter{self, this};
  bool need_wait = false;
  bool deadlock = false;

  bool self_prio_updated = false; // only for need_wait == false
  Thread *chain_prio_updated = nullptr;
  unsigned short chain_prio;
  Thread *fast_prio_updated = nullptr;
  unsigned short fast_prio;

  {
    auto guard = lock_guard<No_cpu_lock_policy>(pi_chain_lock());

    // Note: Read under lock (relaxed memory order).
    Mword old_value = atomic_load_relaxed(_ku_status);

    // TODO: For debug purposes we could let user-space threads pass their
    //       cap_idx, and then add some checks below.
    //       But could be kind of dangerous, as it might be turned into an
    //       arbitrary write primitive, should there ever be a use-after free.

    if (_owner || _owner_died)
      {
        // Thread that already owns the mutex tries to acquire it again?
        if (_owner == self) [[unlikely]]
          return commit_result(-L4_err::EDeadlk); // Deadlock

        if (!setup_wait_timer(self, f->timeout().rcv, utcb, &wait_timeout))
          return commit_error(utcb, L4_error::R_timeout);

        if (!_waiters.first())
          {
            // Set waiter flag.
            // (relaxed memory order sufficient, for details see _ku_status)
            if (!cas_relaxed(_ku_status, old_value,
                             old_value | Pi_waiters_flag)) [[unlikely]]
              // Should never fail, unless user-space illegally tampered with
              // status in the meantime.
              return commit_result(-L4_err::EInval);
          }
        else if (!(old_value & Pi_waiters_flag)) [[unlikely]]
          // User-space messed up waiters flag.
          return commit_result(-L4_err::EInval);

        need_wait =
          prepare_wait_on_pi_mutex(self, self_waiter, &chain_prio_updated,
                                   &chain_prio, &deadlock);
      }
    else
      {
        // With our current implementation it should never be possible to
        // witness a state with waiters but without an owner. Because the
        // transition from owner to the next waiter is done under lock.
        assert(_waiters.first() == nullptr);

        if (old_value & Pi_waiters_flag)
          // Without an owner, the Pi_waiters_flag should never be set,
          // unless user-space illegally tampered with status.
          return commit_result(-L4_err::EInval);

        Mword fast_path_owner_tid = (old_value & Pi_owner_mask);
        if (fast_path_owner_tid)
          {
            if (!setup_wait_timer(self, f->timeout().rcv, utcb, &wait_timeout))
              return commit_error(utcb, L4_error::R_timeout);

            // TODO: Fast path owner that user-space witnessed still holds the
            //       mutex. (needs tid argument)
            //
            // if (fast_path_owner_tid == tid) [[unlikely]]
            //   return commit_result(-L4_err::EDeadlk); // Deadlock

            // (relaxed memory order sufficient, for details see _ku_status)
            if (!cas_relaxed(_ku_status, old_value,
                             old_value | Pi_waiters_flag)) [[unlikely]]
              // Try again. Can fail, in case fast path owner released mutex
              // between us reading status at beginning of the function and now.
              return commit_result(-L4_err::EAgain);

            if (!establish_fast_path_owner(self, fast_path_owner_tid,
                                           &fast_prio_updated, &fast_prio,
                                           &deadlock))
              // Fast path owner was invalid.
              return commit_result(-L4_err::EInval);

            if (!deadlock) [[likely]]
              {
                need_wait = prepare_wait_on_pi_mutex(self, self_waiter,
                                                     &chain_prio_updated,
                                                     &chain_prio, &deadlock);

                // If both establish_fast_path_owner() and
                // prepare_wait_on_pi_mutex() updated prio of same thread, the
                // latter overrides the former.
                if (chain_prio_updated == fast_prio_updated)
                  fast_prio_updated = nullptr;

                // OPTIMIZE: Given that we hold the pi_chain_lock, can it ever
                // be two different threads, and can we maybe even just drop or
                // avoid the former PI chain walk?
              }

          }
        else
          {
            // Fast path owner that user-space witnessed already released the
            // mutex. (Or user-space made the pi_wait call just for fun.)
            // The kernel needs to use a CAS with acquire semantics here,
            // because the previous owner, which acquired and released the mutex
            // via fast path in user-space, did not go through the
            // kernel-internal Spin_lock of the Pi_mutex (see also _ku_status).
            if (!cas_acquire(_ku_status, 0UL,
                             Some_owner_tid | Pi_waiters_flag)) [[unlikely]]
              // Try again. Can fail, in case a user-space thread acquired the
              // free mutex via fast path between us reading status at the
              // beginning of the function and now.
              return commit_result(-L4_err::EAgain);

            // We are the owner now!
            set_owner(self, /* check_dead = */ false);
            self_prio_updated = adjust_prio_on_acquire(self);
          }
      }
  }

  // We encountered a deadlock, at this point everything is cleaned up already.
  // Except that we might have set the Pi_waiters_flag, but that will be cleaned
  // up once the owner releases the mutex.
  if (deadlock) [[unlikely]]
    return commit_result(-L4_err::EDeadlk);

  if (need_wait)
    {
      // Explicitly dequeue from ready queue, so that once we get unblocked we
      // are rescheduled with our new effective priority, see note in
      // _adjust_prio_chain_on_wait().
      Sched_context::rq.current().ready_dequeue(self->sched());
    }
  else
    {
      // Keep effective prio in sync.
      if (self_prio_updated)
        self->ready_queue_update_prio(self->pi_effective_prio());

      // No need to switch to different thread, our prio cannot drop.
    }

  // Update priorities.
  Reschedule need_reschedule = Reschedule::No;
  if (fast_prio_updated)
    need_reschedule |= fast_prio_updated->xcpu_pi_update_prio(fast_prio);
  if (chain_prio_updated)
    need_reschedule |= chain_prio_updated->xcpu_pi_update_prio(chain_prio);

  if (need_reschedule == Reschedule::Yes)
    self->schedule();

  if (need_wait)
    return wait_on_pi_mutex(self, self_waiter, utcb);

  return commit_result(0);
}

/*
 * L4-IFACE: kernel-pi_mutex.pi_mutex-unlock
 * PROTOCOL: L4_PROTO_PI_MUTEX
 */
PRIVATE inline
L4_msg_tag
Pi_mutex::sys_unlock(L4_fpage::Rights, Syscall_frame *f, Utcb const *)
{
  if (f->tag().words() != 1) [[unlikely]]
    return commit_result(-L4_err::EInval);

  Thread *self = current_thread();

  // PI mutex can only be used from the space it was bound to during creation.
  if (self->space() != _space.get()) [[unlikely]]
    return commit_result(-L4_err::EPerm);

  Ref_ptr<Pi_mutex> released_ref;
  bool self_prio_updated;
  Thread *waiter = nullptr;
  unsigned short waiter_prio;

  {
    auto guard = lock_guard<No_cpu_lock_policy>(pi_chain_lock());

    // User-space attempts to unlock mutex that is not locked.
    if (!_owner) [[unlikely]]
      return commit_result(-L4_err::EPerm);

    // Only owner can release mutex.
    if (_owner != self) [[unlikely]]
      return commit_result(-L4_err::EPerm);

    // Need to release from old owner first.
    Thread *old_owner = _owner;
    released_ref = reset_owner();
    self_prio_updated = adjust_prio_on_release(old_owner);

    // Note: Read under lock (relaxed memory order).
    Mword old_value = atomic_load_relaxed(_ku_status);

    Pi_mutex_waiter *waiter_entry = static_cast<Pi_mutex_waiter *>(_waiters.first());
    if (waiter_entry)
      {
        // Transition ownership to top waiter.
        // (relaxed memory order sufficient, for details see _ku_status)
        if (!cas_relaxed(_ku_status, old_value,
                         Some_owner_tid | Pi_waiters_flag)) [[unlikely]]
          // Should never fail, unless user-space illegally tampered with status in
          // the meantime.
          return commit_result(-L4_err::EInval);

        waiter = waiter_entry->thread;
        // First dequeue waiter.
        dequeue_waiter(waiter_entry);
        // Note: Not necessary to adjust priority in PI chain, because we know
        //       that the mutex has no owner, we just unlocked it.

        // Then make it the owner.
        set_owner(waiter, /* check_dead = */ false);
        adjust_prio_on_acquire(waiter);
        waiter_prio = waiter->pi_effective_prio();
      }
    else
      {
        // No waiters, so free the mutex completely (back to fast path).
        // (release memory order required, for details see _ku_status)
        // The kernel needs to use a CAS with release semantics here, because
        // the next owner that acquires the mutex via fast path in user-space
        // will not go through the kernel-internal Spin_lock of the Pi_mutex
        // (see also _ku_status).
        if (!cas_release(_ku_status, old_value, 0UL)) [[unlikely]]
          // Should never fail, unless user-space illegally tampered with status in
          // the meantime.
          return commit_result(-L4_err::EInval);
      }
  }

  if (self_prio_updated)
    self->ready_queue_update_prio(self->pi_effective_prio());

  // NOTE: We must not use the Pi_mutex after any of the switch/schedule
  //       operations below, it might have been deleted in the meantime.

  if (waiter)
    {
      // Waiter always needs prio update, since we only keep the
      // sched_context()->prio() in sync for threads that are non-blocked.
      if (waiter->xcpu_pi_update_prio(waiter_prio, /* make_ready = */ true)
          == Reschedule::Yes)
        {
          self->switch_to_locked(waiter);
          return commit_result(0);
        }
    }

  // We dropped our prio, so might need to switch to different thread.
  Sched_context *next = Sched_context::rq.current().next_to_run();
  if (next->dominates(self->sched()))
    self->schedule();

  return commit_result(0);
}

PUBLIC
void
Pi_mutex::release_owner_on_kill(Context *self) override
{
  Ref_ptr<Pi_mutex> released_ref;
  bool self_prio_updated;

  {
    // Check under lock whether we own or wait on the mutex.
    auto guard = lock_guard<No_cpu_lock_policy>(pi_chain_lock());

    assert(_owner == self);

    // Need to release from old owner first.
    Thread *old_owner = _owner;
    released_ref = reset_owner();
    _owner_died = true;
    self_prio_updated = adjust_prio_on_release(old_owner);

    // Note: Not updating ku_status is fine, the next sys_lock() call will
    //       notice _owner_died and do all cleanup necessary.

    // TODO: Robust mutex support, select next waiter, like we do in sys_unlock,
    // and inform it about died owner.
  }

  if (self_prio_updated)
    self->ready_queue_update_prio(self->pi_effective_prio());

  // We dropped our prio, so might need to switch to different thread.
  Sched_context *next = Sched_context::rq.current().next_to_run();
  if (next->dominates(self->sched()))
    self->schedule();
}

static DEFINE_GLOBAL
Global_data<Kmem_slab_t<Pi_mutex>> _pi_mutex_allocator("PI Mutex");

/**
 * Get the RAM quota of the PI mutex.
 *
 * \return RAM quota.
 */
PUBLIC inline
Ram_quota *
Pi_mutex::ram_quota() const
{
  return _quota;
}

/**
 * PI mutex constructor.
 *
 * \param quota  RAM quota of this object.
 * \param space  Task the PI mutex is bound to.
 * \param space  Kernel-address of mutex status field in kernel-user memory of
 *               bound task.
 */
PUBLIC
Pi_mutex::Pi_mutex(Ram_quota *quota, Space *space, Mword *ku_status)
: _quota(quota), _space(space), _ku_status(ku_status)
{
  // Capability reference, released when last capability of object is released.
  inc_ref();
}

PUBLIC
bool
Pi_mutex::put() override
{
  return dec_ref() == 0;
}

/**
 * PI mutex destructor.
 */
PUBLIC
Pi_mutex::~Pi_mutex() override
{
  // All threads owning or blocking on a Pi_mutex hold a counted reference to
  // the Pi_mutex object. The Pi_mutex object is only deleted once the last
  // thread disconnected and released its reference.
  assert(_owner == nullptr);
  assert(_waiters.first() == nullptr);

  _space.reset();
}

/**
 * Allocate a new PI mutex.
 *
 * The \ref _pi_mutex_allocator slab allocator is used.
 *
 * \param size   Size of the object.
 * \param quota  RAM quota of the object.
 *
 * \return Pointer to the allocated object.
 */
PUBLIC static
void *
Pi_mutex::operator new([[maybe_unused]] size_t size, Ram_quota *quota) noexcept
{
  assert(size == sizeof(Pi_mutex));
  return _pi_mutex_allocator->q_alloc(quota);
}

/**
 * Delete a PI mutex.
 *
 * The \ref _pi_mutex_allocator slab allocator is used.
 *
 * The PI mutex can be safely deleted if the following conditions are met:
 *
 * (1) There are no persistent references to the PI mutex.
 * (2) There are no ephemeral references to the PI mutex (besides the
 *     ephemeral reference that is calling the delete operator).
 *
 * The delete operator is only ever called after the reference count of the
 * PI mutex has dropped to zero. This guarantees that the condition (1) is
 * met.
 *
 * \param pi_mutex  PI mutex to be deleted.
 */
PUBLIC static
void
Pi_mutex::operator delete(Pi_mutex *pi_mutex, std::destroying_delete_t)
{
  Ram_quota *q = pi_mutex->ram_quota();
  pi_mutex->~Pi_mutex();
  _pi_mutex_allocator->q_free(q, pi_mutex);
}

namespace
{

/**
 * Lookup kernel-address of mutex status field in kernel-user memory.
 *
 * \param space         Space of current thread.
 * \param user_address  Address provided by user-space.
 *
 * \return          Kernel-address of mutex status field.
 * \retval nullptr  User-space provided invalid address.
 */
static Mword *
lookup_ku_status(Space *space, Mword user_address)
{
  User_ptr<Mword> status_ptr(reinterpret_cast<Mword *>(user_address));
  Space::Ku_mem const *ku_mem =
    space->find_ku_mem(status_ptr, sizeof(Mword), sizeof(Mword));
  if (!ku_mem) [[unlikely]]
    // Not a valid kumem address.
    return nullptr;

  return ku_mem->kern_addr(status_ptr);
}

/**
 * Factory for creating PI mutexes.
 *
 * \param      quota  RAM quota.
 * \param[out] err    Error code in case of allocation failure.
 *
 * \return PI mutex or nullptr if memory allocation failed.
 */
static Kobject_iface *FIASCO_FLATTEN
pi_mutex_factory(Ram_quota *quota, Space *, L4_msg_tag tag,
                 Utcb const *utcb, Utcb *, int *err, unsigned *)
{
  if (tag.words() != 3 || tag.items() != 1)
    {
      *err = L4_err::EInval;
      return nullptr;
    }

  L4_fpage::Rights task_rights = L4_fpage::Rights(0);
  Task *task(Ko::deref<Task>(&tag, utcb, &task_rights));
  if (!task) [[unlikely]]
    {
      *err = tag.proto();
      return nullptr;
    }

  if (!(task_rights & L4_fpage::Rights::CS())) [[unlikely]]
    {
      *err = -L4_err::EPerm;
      return nullptr;
    }

  if (!(task->caps() & Task::Caps::kumem())) [[unlikely]]
    {
      *err = -L4_err::EInval;
      return nullptr;
    }

  Mword *ku_status = lookup_ku_status(task, utcb->values[2]);
  if (!ku_status)
    {
      // User-space provided invalid kumem address.
      *err = L4_err::EInval;
      return nullptr;
    }

  *err = L4_err::ENomem;
  return new (quota) Pi_mutex(quota, task, ku_status);
}

/**
 * PI mutex factory registration.
 */
static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(pi_mutex_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_pi_mutex, pi_mutex_factory);
}

} // namespace
