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
   * The current programmed timeout.
   */
  Unsigned64 _current;
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
PUBLIC inline NEEDS[Timeout_q::first, "timer.h", "config.h"]
void
Timeout_q::enqueue(Timeout *to)
{
  int queue = (to->_wakeup >> Wakeup_queue_distance) & (Wakeup_queue_count-1);

  To_list &q = first(queue);
  Iterator tmp = q.begin();

  while (tmp != q.end() && tmp->_wakeup < to->_wakeup)
    ++tmp;

  q.insert_before(to, tmp);

  if (Config::Scheduler_one_shot && (to->_wakeup <= _current))
    {
      _current = to->_wakeup;
      Timer::update_timer(_current);
    }
}


/**
 * Timeout constructor.
 */
PUBLIC inline
Timeout::Timeout()
{
  _flags.hit = 0;
  _flags.res = 0;
}


/**
 * Initializes an timeout object.
 */
PUBLIC inline  NEEDS[<climits>]
void
Timeout::init()
{
  _wakeup = ULONG_LONG_MAX;
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


PUBLIC inline NEEDS [<cassert>, "cpu_lock.h", "lock_guard.h",
                     Timeout_q::enqueue, Timeout::is_set]
void
Timeout::set(Unsigned64 clock, Cpu_number cpu)
{
  // XXX uses global kernel lock
  auto guard = lock_guard(cpu_lock);

  assert (!is_set());

  _wakeup = clock;
  Timeout_q::timeout_queue.cpu(cpu).enqueue(this);
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

PUBLIC inline NEEDS [<cassert>, "cpu_lock.h", "lock_guard.h",
                     Timeout::is_set, Timeout_q::enqueue, Timeout::has_hit]
void
Timeout::set_again(Cpu_number cpu)
{
  // XXX uses global kernel lock
  auto guard = lock_guard(cpu_lock);

  assert(! is_set());
  if (has_hit())
    return;

  Timeout_q::timeout_queue.cpu(cpu).enqueue(this);
}

PUBLIC inline NEEDS ["cpu_lock.h", "lock_guard.h", "timer.h",
                     "kdb_ke.h", Timeout::is_set]
void
Timeout::reset()
{
  assert (cpu_lock.test());
  To_list::remove(this);

  // Normaly we should reprogramm the timer in one shot mode
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
 * Dequeue an expired timeout.
 * @return true if a reschedule is necessary, false otherwise.
 */
PUBLIC inline
bool
Timeout::dequeue(bool is_expired = true)
{
  To_list::remove(this);

  if (is_expired)
    return expire();
  else
    return false;
}

/**
 * Handles the timeouts, i.e. call expired() for the expired timeouts
 * and programs the "oneshot timer" to the next timeout.
 * @return true if a reschedule is necessary, false otherwise.
 */
PUBLIC inline NEEDS [<cassert>, <climits>, "kip.h", "timer.h", "config.h",
                     Timeout::expire]
bool
Timeout_q::do_timeouts()
{
  bool reschedule = false;

  // We test if the time between 2 activiations of this function is greater
  // than the distance between two timeout queues.
  // To avoid to lose timeouts, we calculating the missed queues and
  // scan them too.
  // This can only happen, if we dont enter the timer interrupt for a long
  // time, usual with one shot timer.
  // Because we initalize old_dequeue_time with zero,
  // we can get an "miss" on the first timerinterrupt.
  // But this is booting the system, which is uncritical.

  // Calculate which timeout queues needs to be checked.
  int start = (_old_clock >> Wakeup_queue_distance);
  int diff  = (Kip::k()->clock >> Wakeup_queue_distance) - start;
  int end   = (start + diff + 1) & (Wakeup_queue_count - 1);

  // wrap around
  start = start & (Wakeup_queue_count - 1);

  // test if an complete miss
  if (diff >= Wakeup_queue_count)
    start = end = 0; // scan all queues

  // update old_clock for the next run
  _old_clock = Kip::k()->clock;

  // ensure we always terminate
  assert((end >= 0) && (end < Wakeup_queue_count));

  for (;;)
    {
      To_list &q = first(start);
      Iterator timeout = q.begin();

      // now scan this queue for timeouts below current clock
      while (timeout != q.end() && timeout->_wakeup <= (Kip::k()->clock))
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

  if (Config::Scheduler_one_shot)
    {
      // scan all queues for the next minimum
      //_current = (Unsigned64) ULONG_LONG_MAX;
      _current = Kip::k()->clock + 10000; //ms
      bool update_timer = true;

      for (int i = 0; i < Wakeup_queue_count; i++)
	{
	  // make sure that something enqueued other than the dummy element
	  if (first(i).empty())
	    continue;

	  update_timer = true;

	  if (first(i).front()->_wakeup < _current)
	    _current =  first(i).front()->_wakeup;
	}

      if (update_timer)
	Timer::update_timer(_current);

    }
  return reschedule;
}

PUBLIC inline
Timeout_q::Timeout_q()
: _current(ULONG_LONG_MAX), _old_clock(0)
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

