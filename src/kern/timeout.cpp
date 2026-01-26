INTERFACE:

#include <cxx/hlist>
#include "l4_types.h"
#include "per_cpu_data.h"

/** A timeout basic object. It contains the necessary queues and handles
    enqueuing, dequeuing and handling of timeouts. Real timeout classes
    should overwrite expired(), which will do the real work, if an
    timeout hits.
 */
class Timeout : public cxx::H_list_item
{
  friend class Jdb_timeout_list;
  friend class Jdb_list_timeouts;
  friend class Timeout_q;
  friend class Timeouts_test;

public:
  typedef cxx::H_list<Timeout> To_list;

protected:
  /**
   * Absolute system time we want to be woken up at.
   */
  Unsigned64 _wakeup;

private:
  /**
   * Default copy constructor (is undefined).
   */
  Timeout(const Timeout&);

  /**
   * Overwritten timeout handler function.
   * @return true if a reschedule is necessary, false otherwise.
   */
  virtual bool expired();

  struct
  {
    bool     hit  : 1;
    unsigned res  : 6; // performance optimization
  } _flags;
};


class Timeout_q
{
  friend class Timeouts_test;
private:
  /**
   * Timeout queue count (2^n) and  distance between two queues in 2^n.
   */
  enum
  {
    Wakeup_queue_count	  = 8,
    Wakeup_queue_distance = 12 // i.e. (1<<12)us
  };

  typedef Timeout::To_list To_list;
  typedef To_list::Iterator Iterator;
  typedef To_list::Const_iterator Const_iterator;

  /**
   * The timeout queues.
   */
  To_list _q[Wakeup_queue_count];

  /**
   * The current programmed timeout (only used in one-shot mode).
   */
  Unsigned64 _current;
  /**
   * The time when timeouts were last handled by Timeout_q::do_timeouts().
   */
  Unsigned64 _old_clock;

public:
  static Per_cpu<Timeout_q> timeout_queue;
};

//----------------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>
#include "cpu_lock.h"
#include "kip.h"
#include "lock_guard.h"
#include "timer.h"
#include "static_init.h"
#include <climits>
#include "config.h"
#include "kdb_ke.h"


DEFINE_PER_CPU Per_cpu<Timeout_q> Timeout_q::timeout_queue;


PUBLIC inline
Timeout_q::To_list &
Timeout_q::first(int index)
{ return _q[index & (Wakeup_queue_count-1)]; }

PUBLIC inline
Timeout_q::To_list const &
Timeout_q::first(int index) const
{ return _q[index & (Wakeup_queue_count-1)]; }

PUBLIC inline
unsigned
Timeout_q::queues() const { return Wakeup_queue_count; }

/**
 * Enqueue a new timeout.
 */
PUBLIC inline NEEDS[Timeout_q::first, Timeout_q::program_timer, "config.h"]
void
Timeout_q::enqueue(Timeout *to)
{
  int queue = (to->_wakeup >> Wakeup_queue_distance) & (Wakeup_queue_count-1);

  To_list &q = first(queue);
  Iterator tmp = q.begin();

  while (tmp != q.end() && tmp->_wakeup < to->_wakeup)
    ++tmp;

  q.insert_before(to, tmp);

  if constexpr (Config::Scheduler_one_shot)
    {
      if (to->_wakeup < _current)
        program_timer(to->_wakeup);
    }
}


/**
 * Timeout constructor.
 */
PUBLIC inline
Timeout::Timeout()
{
  _wakeup    = ULLONG_MAX;
  _flags.hit = 0;
  _flags.res = 0;
}


/* Yeah, i know, an derived and specialized timeout class for
   the root node would be nicer. I already had done this, but
   it was significantly slower than this solution */
IMPLEMENT virtual bool
Timeout::expired()
{
  kdb_ke("Wakeup List Terminator reached");
  return false;
}

/**
 * Check if timeout is set.
 */
PUBLIC inline
bool
Timeout::is_set()
{
  return To_list::in_list(this);
}

/**
 * Check if timeout has hit.
 */
PUBLIC inline
bool
Timeout::has_hit()
{
  return _flags.hit;
}

/**
 * Program timeout to expire at the specified wakeup time on the current CPU.
 *
 * \param clock  Wakeup time
 *
 * \pre `cpu_lock` must be held
 * \pre Timeout must not be set
 */
PUBLIC inline NEEDS [<cassert>, "cpu_lock.h", "lock_guard.h",
                     Timeout_q::enqueue, Timeout::is_set]
void
Timeout::set(Unsigned64 clock)
{
  assert(cpu_lock.test());
  assert(!is_set());

  _wakeup = clock;
  Timeout_q::timeout_queue.current().enqueue(this);
}

/**
 * Return remaining time of timeout.
 */
PUBLIC inline
Signed64
Timeout::get_timeout(Unsigned64 clock)
{
  return _wakeup - clock;
}

/**
 * Program reset timeout to expire at the originally set wakeup time on the
 * current CPU, unless it already has been hit.
 *
 * \pre `cpu_lock` must be held
 * \pre Timeout must not be set
 */
PUBLIC inline NEEDS [<cassert>, "cpu_lock.h", "lock_guard.h",
                     Timeout::is_set, Timeout_q::enqueue, Timeout::has_hit]
void
Timeout::set_again()
{
  assert(cpu_lock.test());
  assert(!is_set());

  if (has_hit())
    return;

  Timeout_q::timeout_queue.current().enqueue(this);
}

/**
 * Reset timeout, preventing its expiration.
 *
 * \pre `cpu_lock` must be held
 */
PUBLIC inline NEEDS [<cassert>, "cpu_lock.h"]
void
Timeout::reset()
{
  assert (cpu_lock.test());
  To_list::remove(this);

  // Normally we should reprogram the timer in one shot mode
  // But we let the timer interrupt handler to do this "lazily", to save cycles
}

/**
 * Dequeue an expired timeout.
 * @return true if a reschedule is necessary, false otherwise.
 */
PRIVATE inline
bool
Timeout::expire()
{
  _flags.hit = 1;
  return expired();
}

/**
 * Program the timer interrupt to the given timeout.
 *
 * Enforces a lower bound for the timeout, as defined by
 * Config::One_shot_min_interval_us, to prevent too frequent handling of the
 * timeout queue.
 */
PRIVATE inline NEEDS ["config.h", "timer.h"]
void
Timeout_q::program_timer(Unsigned64 timeout)
{
   if (timeout < _old_clock + Config::One_shot_min_interval_us)
    _current = _old_clock + Config::One_shot_min_interval_us;
  else
    _current = timeout;

  Timer::update_timer(_current);
}

/**
 * Update the timer interrupt to the next timeout.
 *
 * Set the timer to the next timeout in one-shot mode. The parameter
 * gives a hint for the maximum timeout to set.
 *
 * \param max_timeout  Maximum timeout to set.
 *                     Use Timer::Infinite_timeout for no timeout.
 */
PUBLIC inline NEEDS ["config.h", Timeout_q::program_timer]
void
Timeout_q::update_timer(Unsigned64 max_timeout, Timeout const *ignore = nullptr)
{
  if constexpr (Config::Scheduler_one_shot)
    program_timer(next_timeout(max_timeout, ignore));
}

/**
 * Handles the timeouts by calling expired() for the expired timeouts and
 * programming the "oneshot timer" to the next timeout.
 *
 * \param now  The system clock used to decide if timeouts have expired.
 *
 * \retval true if a reschedule is necessary.
 * \retval false otherwise.
 */
PUBLIC inline NEEDS [<cassert>, <climits>, "timer.h", "config.h",
                     Timeout::expire]
bool
Timeout_q::do_timeouts(Unsigned64 now)
{
  bool reschedule = false;

  // We test if the time between 2 activations of this function is greater
  // than the distance between two timeout queues.
  // To avoid losing timeouts, we calculate the missed queues and scan them
  // too.
  // This can only happen, if we don't enter the timer interrupt for a long
  // time, usual with one-shot timer.
  // Because we initialize old_dequeue_time with zero,
  // we can get a "miss" on the first timer interrupt.
  // But this is booting the system, which is uncritical.

  // Calculate which timeout queues needs to be checked.
  int start = (_old_clock >> Wakeup_queue_distance);
  int diff  = (now >> Wakeup_queue_distance) - start;
  int end   = (start + diff + 1) & (Wakeup_queue_count - 1);

  // wrap around
  start = start & (Wakeup_queue_count - 1);

  // test if an complete miss
  if (diff >= Wakeup_queue_count)
    start = end = 0; // scan all queues

  // update old_clock for the next run
  _old_clock = now;

  // ensure we always terminate
  assert((end >= 0) && (end < Wakeup_queue_count));

  for (;;)
    {
      To_list &q = first(start);
      Iterator timeout = q.begin();

      // now scan this queue for timeouts below current clock
      while (timeout != q.end() && timeout->_wakeup <= now)
        {
          Timeout *to = *timeout;
          timeout = q.erase(timeout);
          reschedule |= to->expire();
        }

      // next queue
      start = (start + 1) & (Wakeup_queue_count - 1);

      if (start == end)
	break;
    }

  // The timer interrupt performs some house-keeping, e.g. it keeps RCU running.
  // Make sure it has a defined maximum latency.
  if constexpr (Config::Need_rcu_tick)
    update_timer(now + Config::Rcu_grace_period);
  else
    update_timer(Timer::Infinite_timeout);

  return reschedule;
}

PUBLIC inline
Timeout_q::Timeout_q()
: _current(ULLONG_MAX), _old_clock(0)
{}

PUBLIC inline
bool
Timeout_q::have_timeouts(Timeout const *ignore) const
{
  for (unsigned i = 0; i < Wakeup_queue_count; ++i)
    {
      To_list const &t = first(i);
      if (!t.empty())
        {
          To_list::Const_iterator f = t.begin();
          if (*f == ignore && (++f) == t.end())
            continue;

          return true;
        }
    }

  return false;
}

PUBLIC inline
Unsigned64
Timeout_q::next_timeout(Unsigned64 max_timeout, Timeout const *ignore) const
{
  Unsigned64 next = max_timeout;

  // scan all queues for the next minimum
  for (int i = 0; i < Wakeup_queue_count; i++)
    {
      To_list const &queue = first(i);
      // make sure that something enqueued other than the dummy element
      if (queue.empty())
        continue;

      To_list::Const_iterator timeout = queue.begin();
      if (ignore != nullptr && (*timeout == ignore && ++timeout == queue.end()))
        continue;

      if (timeout->_wakeup < next)
        next = timeout->_wakeup;
    }

  return next;
}
