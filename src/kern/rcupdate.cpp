INTERFACE:

#include "cpu_mask.h"
#include "per_cpu_data.h"
#include "spin_lock.h"
#include <cxx/slist>

class Rcu_glbl;
class Rcu_data;

/**
 * \brief Encapsulation of RCU batch number.
 */
class Rcu_batch
{
  friend class Jdb_rcupdate;
public:
  /// create uninitialized batch.
  Rcu_batch() {}
  /// create a btach initialized with \a b.
  Rcu_batch(long b) : _b(b) {}

  /// less than comparison.
  bool operator < (Rcu_batch const &o) const { return (_b - o._b) < 0; }
  /// greater than comparison.
  bool operator > (Rcu_batch const &o) const { return (_b - o._b) > 0; }
  /// greater than comparison.
  bool operator >= (Rcu_batch const &o) const { return (_b - o._b) >= 0; }
  /// equelity check.
  bool operator == (Rcu_batch const &o) const { return _b == o._b; }
  /// inequality test.
  bool operator != (Rcu_batch const &o) const { return _b != o._b; }
  /// increment batch.
  Rcu_batch &operator ++ () { ++_b; return *this; }
  /// increase batch with \a r.
  Rcu_batch operator + (long r) { return Rcu_batch(_b + r); }


private:
  long _b;
};

/**
 * \brief Item that can bequeued for the next grace period.
 *
 * An RCU item is basically a pointer to a callback which is called
 * after one grace period.
 */
class Rcu_item : public cxx::S_list_item
{
  friend class Rcu_data;
  friend class Rcu;
  friend class Jdb_rcupdate;

private:
  bool (*_call_back)(Rcu_item *);
};


/**
 * \brief List of Rcu_items.
 *
 * RCU lists are used a lot of times in the RCU implementation and are
 * implemented as single linked lists with FIFO semantics.
 *
 * \note Concurrent access to the list is not synchronized.
 */
class Rcu_list : public cxx::S_list_tail<Rcu_item>
{
private:
  typedef cxx::S_list_tail<Rcu_item> Base;
public:
  Rcu_list() {}
  Rcu_list(Rcu_list &&o) : Base(static_cast<Base &&>(o)) {}
  Rcu_list &operator = (Rcu_list &&o)
  {
    Base::operator = (static_cast<Base &&>(o));
    return *this;
  }

private:
  friend class Jdb_rcupdate;
};

/**
 * \brief CPU local data structure for RCU.
 */
class Rcu_data
{
  friend class Jdb_rcupdate;
public:

  Rcu_batch _q_batch;   ///< batch nr. for grace period
  bool _q_passed;       ///< quiescent state passed?
  bool _pending;        ///< wait for quiescent state
  bool _idle;

  Rcu_batch _batch;
  Rcu_list _n;
  long _len;
  Rcu_list _c;
  Rcu_list _d;
  Cpu_number _cpu;
};


/**
 * \brief Global RCU data structure.
 */
class Rcu_glbl
{
  friend class Rcu_data;
  friend class Rcu;
  friend class Jdb_rcupdate;

private:
  Rcu_batch _current;      ///< current batch
  Rcu_batch _completed;    ///< last completed batch
  bool _next_pending;      ///< next batch already pending?
  Spin_lock<> _lock;
  Cpu_mask _cpus;

  Cpu_mask _active_cpus;

};

/**
 * \brief encapsulation of RCU implementation.
 *
 * This class aggregates per CPU data structures as well as the global
 * data structure for RCU and provides a common RCU interface.
 */
class Rcu
{
  friend class Rcu_data;
  friend class Jdb_rcupdate;

public:
  /// The lock to prevent a quiescent state.
  typedef Cpu_lock Lock;
  static Rcu_glbl *rcu() { return &_rcu; }
private:
  static Rcu_glbl _rcu;
  static Per_cpu<Rcu_data> _rcu_data;
};

// ------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Rcu
{
public:
  struct Log_rcu : public Tb_entry
  {
    Cpu_number cpu;
    Rcu_item *item;
    void *cb;
    unsigned char event;
    void print(String_buffer *buf) const;
  };

  enum Rcu_events
  {
    Rcu_call = 0,
    Rcu_process = 1,
  };
};


// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "logdefs.h"
#include "string_buffer.h"

IMPLEMENT
void
Rcu::Log_rcu::print(String_buffer *buf) const
{
  char const *events[] = { "call", "process"};
  buf->printf("rcu-%s (cpu=%u) item=%p", events[event],
              cxx::int_value<Cpu_number>(cpu), item);
}


//--------------------------------------------------------------------------
IMPLEMENTATION:

#include "cpu.h"
#include "cpu_lock.h"
#include "globals.h"
#include "lock_guard.h"
#include "mem.h"
#include "static_init.h"
#include "timeout.h"
#include "logdefs.h"

// XXX: includes for debugging
// #include "logdefs.h"


class Rcu_timeout : public Timeout
{
};

/**
 * Timeout expiration callback function
 * @return true if reschedule is necessary, false otherwise
 */
PRIVATE
bool
Rcu_timeout::expired()
{ return Rcu::process_callbacks(); }


Rcu_glbl Rcu::_rcu INIT_PRIORITY(EARLY_INIT_PRIO);
DEFINE_PER_CPU Per_cpu<Rcu_data> Rcu::_rcu_data(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU static Per_cpu<Rcu_timeout> _rcu_timeout;

PUBLIC
Rcu_glbl::Rcu_glbl()
: _current(-300),
  _completed(-300)
{}

PUBLIC
Rcu_data::Rcu_data(Cpu_number cpu)
: _idle(true),
  _cpu(cpu)
{}


/**
 * \brief Enqueue Rcu_item into the list (at the tail).
 * \prarm i the RCU item to enqueue.
 */
PUBLIC inline void Rcu_list::enqueue(Rcu_item *i){ push_back(i); }

/**
 * \pre must run under cpu lock
 */
PUBLIC inline
void
Rcu_data::enqueue(Rcu_item *i)
{
  _n.enqueue(i);
  ++_len;
}

PRIVATE inline NOEXPORT NEEDS["cpu_lock.h", "lock_guard.h"]
bool
Rcu_data::do_batch()
{
  int count = 0;
  bool need_resched = false;
  for (Rcu_list::Const_iterator l = _d.begin(); l != _d.end();)
    {
      Rcu_item *i = *l;
      ++l;

      need_resched |= i->_call_back(i);
      ++count;
    }

  // XXX: I do not know why this and the former stuff is w/o cpu lock
  //      but the couting needs it?
  _d.clear();

  // XXX: we use clear, we seemingly worked through the whole list
  //_d.head(l);

    {
      auto guard = lock_guard(cpu_lock);
      _len -= count;
    }

  return need_resched;
}

PRIVATE inline NOEXPORT
void
Rcu_glbl::start_batch()
{
  if (_next_pending && _completed == _current)
    {
      _next_pending = 0;
      Mem::mp_wmb();
      ++_current;
      Mem::mp_mb();
      _cpus = _active_cpus;
    }
}

PUBLIC
void
Rcu_data::enter_idle(Rcu_glbl *rgp)
{
  if (EXPECT_TRUE(!_idle))
    {
      _idle = true;

      auto guard = lock_guard(rgp->_lock);
      rgp->_active_cpus.clear(_cpu);

      if (_q_batch != rgp->_current || _pending)
        {
          _q_batch = rgp->_current;
          _pending = 0;
          rgp->cpu_quiet(_cpu);
          assert (!pending(rgp));
        }
    }
}

PUBLIC static inline
void
Rcu::enter_idle(Cpu_number cpu)
{
  Rcu_data *rdp = &_rcu_data.cpu(cpu);
  rdp->enter_idle(rcu());
}

PUBLIC static inline
void
Rcu::leave_idle(Cpu_number cpu)
{
  Rcu_data *rdp = &_rcu_data.cpu(cpu);
  if (EXPECT_FALSE(rdp->_idle))
    {
      rdp->_idle = false;
      auto guard = lock_guard(rcu()->_lock);
      rcu()->_active_cpus.set(cpu);
      rdp->_q_batch = Rcu::rcu()->_current;
    }
}


PRIVATE inline NOEXPORT
void
Rcu_glbl::cpu_quiet(Cpu_number cpu)
{
  _cpus.clear(cpu);
  if (_cpus.empty())
    {
      _completed = _current;
      start_batch();
    }
}

PRIVATE
void
Rcu_data::check_quiescent_state(Rcu_glbl *rgp)
{
  if (_q_batch != rgp->_current)
    {
      // start new grace period
      _pending = 1;
      _q_passed = 0;
      _q_batch = rgp->_current;
      return;
    }

  // Is the grace period already completed for this cpu?
  // use _pending, not bitmap to avoid cache trashing
  if (!_pending)
    return;

  // Was there a quiescent state since the beginning of the grace period?
  if (!_q_passed)
    return;

  _pending = 0;

  auto guard = lock_guard(rgp->_lock);

  if (EXPECT_TRUE(_q_batch == rgp->_current))
    rgp->cpu_quiet(_cpu);
}


PUBLIC static //inline NEEDS["cpu_lock.h", "globals.h", "lock_guard.h", "logdefs.h"]
void
Rcu::call(Rcu_item *i, bool (*cb)(Rcu_item *))
{
  i->_call_back = cb;
  LOG_TRACE("Rcu call", "rcu", ::current(), Log_rcu,
      l->cpu   = current_cpu();
      l->event = Rcu_call;
      l->item = i;
      l->cb = (void*)cb);

  auto guard = lock_guard(cpu_lock);

  Rcu_data *rdp = &_rcu_data.current();
  rdp->enqueue(i);
}

PRIVATE
void
Rcu_data::move_batch(Rcu_list &l)
{
  auto guard = lock_guard(cpu_lock);
  _n.append(l);
}


PUBLIC
Rcu_data::~Rcu_data()
{
  if (current_cpu() == _cpu)
    return;

  Rcu_data *current_rdp = &Rcu::_rcu_data.current();
  Rcu_glbl *rgp = Rcu::rcu();

    {
      auto guard = lock_guard(rgp->_lock);
      if (rgp->_current != rgp->_completed)
	rgp->cpu_quiet(_cpu);
    }

  current_rdp->move_batch(_c);
  current_rdp->move_batch(_n);
  current_rdp->move_batch(_d);
}

PUBLIC
bool FIASCO_WARN_RESULT
Rcu_data::process_callbacks(Rcu_glbl *rgp)
{
  LOG_TRACE("Rcu callbacks", "rcu", ::current(), Rcu::Log_rcu,
      l->cpu = _cpu;
      l->item = 0;
      l->event = Rcu::Rcu_process);

  if (!_c.empty() && rgp->_completed >= _batch)
    _d.append(_c);

  if (!_n.empty() && _c.empty())
    {
	{
	  auto guard = lock_guard(cpu_lock);
	  _c = cxx::move(_n);
	}

      // start the next batch of callbacks

      _batch = rgp->_current + 1;
      Mem::mp_rmb();

      if (!rgp->_next_pending)
	{
	  // start the batch and schedule start if it's a new batch
	  auto guard = lock_guard(rgp->_lock);
	  rgp->_next_pending = 1;
	  rgp->start_batch();
	}
    }

  check_quiescent_state(rgp);
  if (!_d.empty())
    return do_batch();

  return false;
}

PUBLIC inline
bool
Rcu_data::pending(Rcu_glbl *rgp) const
{
  // The CPU has pending RCU callbacks and the grace period for them
  // has been completed.
  if (!_c.empty() && rgp->_completed >= _batch)
    return 1;

  // The CPU has no pending RCU callbacks, however there are new callbacks
  if (_c.empty() && !_n.empty())
    return 1;

  // The CPU has callbacks to be invoked finally
  if (!_d.empty())
    return 1;

  // RCU waits for a quiescent state from the CPU
  if ((_q_batch != rgp->_current) || _pending)
    return 1;

  // OK, no RCU work to do
  return 0;

}

PUBLIC static inline NEEDS["globals.h"]
bool FIASCO_WARN_RESULT
Rcu::process_callbacks()
{ return _rcu_data.current().process_callbacks(&_rcu); }

PUBLIC static inline NEEDS["globals.h"]
bool FIASCO_WARN_RESULT
Rcu::process_callbacks(Cpu_number cpu)
{ return _rcu_data.cpu(cpu).process_callbacks(&_rcu); }

PUBLIC static inline
bool
Rcu::pending(Cpu_number cpu)
{
  return _rcu_data.cpu(cpu).pending(&_rcu);
}

PUBLIC static inline
bool
Rcu::idle(Cpu_number cpu)
{
  Rcu_data const *d = &_rcu_data.cpu(cpu);
  return d->_c.empty() && !d->pending(&_rcu);
}

PUBLIC static inline
void
Rcu::inc_q_cnt(Cpu_number cpu)
{ _rcu_data.cpu(cpu)._q_passed = 1; }

PUBLIC static
void
Rcu::schedule_callbacks(Cpu_number cpu, Unsigned64 clock)
{
  Timeout *t = &_rcu_timeout.cpu(cpu);
  if (!t->is_set())
    t->set(clock, cpu);
}

PUBLIC static inline NEEDS["cpu_lock.h"]
Rcu::Lock *
Rcu::lock()
{ return &cpu_lock; }


PUBLIC static inline
bool
Rcu::do_pending_work(Cpu_number cpu)
{
  if (pending(cpu))
    {
      inc_q_cnt(cpu);
      return process_callbacks(cpu);
    }
  return false;
}
