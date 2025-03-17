INTERFACE[mp]:

#include "cpu_mask.h"
#include "per_cpu_data.h"
#include "spin_lock.h"
#include <cxx/slist>
#include "global_data.h"

class Rcu_glbl;
class Rcu_data;

/**
 * Encapsulation of RCU batch number.
 */
class Rcu_batch
{
  friend class Jdb_rcupdate;
public:
  /// create a batch initialized with \a b.
  Rcu_batch(long b) : _b(b) {}

  /// less than comparison.
  bool operator < (Rcu_batch const &o) const { return (_b - o._b) < 0; }
  /// greater than comparison.
  bool operator > (Rcu_batch const &o) const { return (_b - o._b) > 0; }
  /// greater than / equal to comparison.
  bool operator >= (Rcu_batch const &o) const { return (_b - o._b) >= 0; }
  /// equality check.
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
 * Item that can bequeued for the next grace period.
 *
 * An RCU item is basically a pointer to a callback which is called
 * after one grace period.
 */
class Rcu_item : public cxx::S_list_item
{
  friend class Rcu_data;
  friend class Rcu;
  friend class Jdb_rcupdate;
  friend class Rcu_tester;

private:
  bool (*_call_back)(Rcu_item *) = nullptr;
};


/**
 * List of Rcu_items.
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
 * CPU local data structure for RCU.
 */
class Rcu_data
{
  friend class Jdb_rcupdate;
public:

  Rcu_batch _q_batch;     ///< batch no. for grace period
  bool _q_passed = false; ///< quiescent state for batch `_q_batch` passed?
  bool _pending  = false; ///< waiting for quiescent state for batch `_q_batch`?
  bool _idle     = true;  ///< `false` iff CPU `_cpu` is participating in RCU

  Rcu_batch _batch;       ///< batch no. assigned to the items in `_c`
  Rcu_list _n;            ///< new items: waiting for being assigned to a batch
  long _len = 0;          ///< number of items in `_n + _c + _d` (for debugging)
  Rcu_list _c;            ///< current items: assigned to batch `_batch`
  Rcu_list _d;            ///< done items: waited a full grace period
  Cpu_number _cpu;        ///< the CPU this `Rcu_data` instance is assigned to
};


/**
 * Global RCU data structure.
 */
class Rcu_glbl
{
  friend class Rcu_data;
  friend class Rcu;
  friend class Jdb_rcupdate;

private:
  Rcu_batch _current;         ///< current batch
  Rcu_batch _completed;       ///< last completed batch
  bool _next_pending = false; ///< Are there items in batch `_current + 1`?
  Spin_lock<> _lock;
  Cpu_mask _cpus;             ///< CPUs waiting for a quiescent state

  Cpu_mask _active_cpus;      ///< CPUs participating in RCU

};

/**
 * Encapsulation of RCU implementation.
 *
 * This class aggregates per CPU data structures as well as the global
 * data structure for RCU and provides a common RCU interface.
 */
class Rcu
{
  friend class Rcu_data;
  friend class Jdb_rcupdate;
  friend class Rcu_tester;

public:
  /// The lock to prevent a quiescent state.
  typedef Cpu_lock Lock;
  static Rcu_glbl *rcu() { return &_rcu; }
private:
  static Global_data<Rcu_glbl> _rcu;
  static Per_cpu<Rcu_data> _rcu_data;
};

// ------------------------------------------------------------------------
INTERFACE [mp && debug]:

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
  static_assert(sizeof(Log_rcu) <= Tb_entry::Tb_entry_size);

  enum Rcu_events
  {
    Rcu_call = 0,
    Rcu_process = 1,
    Rcu_idle = 2,
    Rcu_unidle = 3,
  };
};

// ------------------------------------------------------------------------
INTERFACE [!mp]:

class Rcu_item {};

class Rcu
{
public:
  static inline void enter_idle(Cpu_number) {};
  static inline void leave_idle(Cpu_number) {};
  static inline bool has_pending_work(Cpu_number) { return false; };
  static inline bool do_pending_work(Cpu_number) { return false; }
};

// --------------------------------------------------------------------------
IMPLEMENTATION [mp && debug]:

#include "logdefs.h"
#include "string_buffer.h"

IMPLEMENT
void
Rcu::Log_rcu::print(String_buffer *buf) const
{
  static constinit char const *const events[] =
  { "call", "process", "idle", "unidle" };
  buf->printf("rcu-%s (cpu=%u) item=%p", events[event],
              cxx::int_value<Cpu_number>(cpu), static_cast<void *>(item));
}


//--------------------------------------------------------------------------
IMPLEMENTATION[mp]:

#include "cpu.h"
#include "cpu_lock.h"
#include "globals.h"
#include "lock_guard.h"
#include "mem.h"
#include "static_init.h"
#include "logdefs.h"


DEFINE_GLOBAL_PRIO(EARLY_INIT_PRIO) Global_data<Rcu_glbl> Rcu::_rcu;
DEFINE_PER_CPU Per_cpu<Rcu_data> Rcu::_rcu_data(Per_cpu_data::Cpu_num);

PUBLIC
Rcu_glbl::Rcu_glbl()
: _current(-300),
  _completed(-300)
{}

PUBLIC
Rcu_data::Rcu_data(Cpu_number cpu)
: _q_batch(0), _batch(0), _cpu(cpu)
{}


/**
 * Enqueue Rcu_item into the list (at the tail).
 *
 * \param i the RCU item to enqueue.
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
  // This function may not properly work if called without CPU lock.
  assert(cpu_lock.test());

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
  //      but the counting needs it?
  //      Actually this function is always called with taken CPU lock.
  _d.clear();

  // XXX: we use clear, we seemingly worked through the whole list
  //_d.head(l);

    {
      // auto guard = lock_guard(cpu_lock); -- See function precondition
      _len -= count;
    }

  return need_resched;
}

/**
 * Start a new grace period if there are callbacks queued for the next batch and
 * the current batch is completed.
 *
 * \pre #_lock must be locked
 */
PRIVATE inline NOEXPORT
void
Rcu_glbl::start_batch()
{
  if (_next_pending && _completed == _current)
    {
      _next_pending = false;
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
      LOG_TRACE("Rcu idle", "rcu", ::current(), Rcu::Log_rcu,
          l->cpu = _cpu;
          l->item = nullptr;
          l->event = Rcu::Rcu_idle);

      _idle = true;
      auto guard = lock_guard(rgp->_lock);
      rgp->_active_cpus.clear(_cpu);

      if (_q_batch != rgp->_current || _pending)
        {
          _q_batch = rgp->_current;
          _pending = false;
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

PUBLIC static inline NEEDS["logdefs.h", "lock_guard.h"]
void
Rcu::leave_idle(Cpu_number cpu)
{
  Rcu_data *rdp = &_rcu_data.cpu(cpu);
  if (EXPECT_FALSE(rdp->_idle))
    {
      LOG_TRACE("Rcu idle", "rcu", ::current(), Rcu::Log_rcu,
          l->cpu = cpu;
          l->item = nullptr;
          l->event = Rcu::Rcu_unidle);

      rdp->_idle = false;
      auto guard = lock_guard(rcu()->_lock);
      rcu()->_active_cpus.set(cpu);
      rdp->_q_batch = Rcu::rcu()->_current;
    }
}


/**
 * Announce a quiescent state of a CPU.
 *
 * If no CPU is left waiting for a quiescent state, the current grace period is
 * finished and the current batch is completed. If additionally there are
 * callbacks queued for the next batch (#_next_pending), a new grace period is
 * started (cf. start_batch()).
 *
 * \pre #_lock must be locked
 */
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
      _pending = true;
      _q_passed = false;
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

  _pending = false;

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
      l->cb = reinterpret_cast<void*>(cb));

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
[[nodiscard]] bool
Rcu_data::process_callbacks(Rcu_glbl *rgp)
{
  // This function may not work properly if called without CPU lock.
  assert(cpu_lock.test());

  LOG_TRACE("Rcu callbacks", "rcu", ::current(), Rcu::Log_rcu,
      l->cpu = _cpu;
      l->item = nullptr;
      l->event = Rcu::Rcu_process);

  if (!_c.empty() && rgp->_completed >= _batch)
    _d.append(_c);

  if (!_n.empty() && _c.empty())
    {
	{
	  // auto guard = lock_guard(cpu_lock); -- See function precondition.
	  _c = cxx::move(_n);
	}

      // start the next batch of callbacks

      _batch = rgp->_current + 1;
      Mem::mp_rmb();

      if (!rgp->_next_pending)
	{
	  // start the batch and schedule start if it's a new batch
	  auto guard = lock_guard(rgp->_lock);
	  rgp->_next_pending = true;
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

PUBLIC [[nodiscard]] static inline NEEDS["globals.h"]
bool
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
Rcu::has_pending_work(Cpu_number cpu)
{
  Rcu_data const *d = &_rcu_data.cpu(cpu);
  return !d->_c.empty() || d->pending(&_rcu);
}

PUBLIC static inline
void
Rcu::inc_q_cnt(Cpu_number cpu)
{ _rcu_data.cpu(cpu)._q_passed = true; }

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
