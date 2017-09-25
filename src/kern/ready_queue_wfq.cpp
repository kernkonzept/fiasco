INTERFACE[sched_wfq || sched_fp_wfq]:

#include "member_offs.h"
#include "types.h"
#include "globals.h"

struct L4_sched_param_wfq : public L4_sched_param
{
  enum : Smword { Class = -2 };
  Mword quantum;
  Mword weight;
};

template< typename E >
class Ready_queue_wfq
{
  friend class Jdb_thread_list;
  template<typename T>
  friend class Jdb_thread_list_policy;

public:
  E *current_sched() const { return _current_sched; }
  void activate(E *s) { _current_sched = s; }
  E *idle;

  void set_idle(E *sc)
  {
    idle = sc;
    _e(sc)->_ready_link = &idle;
    _e(sc)->_idle = 1;
  }

  void enqueue(E *, bool is_current);
  void dequeue(E *);
  E *next_to_run() const;

private:
  void swap(unsigned a, unsigned b);
  void heap_up(unsigned a);
  void heap_down(unsigned a);

  E *_current_sched;
  unsigned _cnt;
  E *_heap[1024];

  static typename E::Wfq_sc *_e(E *e) { return E::wfq_elem(e); }
};

template< typename IMPL >
class Sched_context_wfq
{
public:
  bool operator <= (Sched_context_wfq const &o) const
  { return _impl()._dl <= o._impl()._dl; }

  bool operator < (Sched_context_wfq const &o) const
  { return _impl()._dl < o._impl()._dl; }

private:
  IMPL const &_impl() const { return static_cast<IMPL const &>(*this); }
  IMPL &_impl() { return static_cast<IMPL &>(*this); }
};


// --------------------------------------------------------------------------
IMPLEMENTATION [sched_wfq || sched_fp_wfq]:

#include <cassert>
#include "config.h"
#include "cpu_lock.h"
#include "std_macros.h"


IMPLEMENT inline
template<typename E>
E *
Ready_queue_wfq<E>::next_to_run() const
{
  if (_cnt)
    return _heap[0];

  if (_current_sched)
    _e(idle)->_dl = _e(_current_sched)->_dl;

  return idle;
}

IMPLEMENT inline
template<typename E>
void
Ready_queue_wfq<E>::swap(unsigned a, unsigned b)
{
  _e(_heap[a])->_ready_link = &_heap[b];
  _e(_heap[b])->_ready_link = &_heap[a];
  E *s = _heap[a];
  _heap[a] = _heap[b];
  _heap[b] = s;
}

IMPLEMENT inline
template<typename E>
void
Ready_queue_wfq<E>::heap_up(unsigned a)
{
  for (;a > 0;)
    {
      unsigned p = (a-1)/2;
      if (*_e(_heap[p]) < *_e(_heap[a]))
	return;
      swap(p, a);
      a = p;
    }
}

IMPLEMENT inline
template<typename E>
void
Ready_queue_wfq<E>::heap_down(unsigned a)
{
  for (;;)
    {
      unsigned c1 = 2*a + 1;
      unsigned c2 = 2*a + 2;

      if (_cnt <= c1)
	return;

      if (_cnt > c2 && *_e(_heap[c2]) <= *_e(_heap[c1]))
	c1 = c2;

      if (*_e(_heap[a]) <= *_e(_heap[c1]))
	return;

      swap(c1, a);

      a = c1;
    }
}

/**
 * Enqueue context in ready-list.
 */
IMPLEMENT
template<typename E>
void
Ready_queue_wfq<E>::enqueue(E *i, bool /*is_current_sched**/)
{
  assert(cpu_lock.test());

  // Don't enqueue threads which are already enqueued
  if (EXPECT_FALSE (i->in_ready_list()))
    return;

  unsigned n = _cnt++;

  E *&h = _heap[n];
  h = i;
  _e(i)->_ready_link = &h;

  heap_up(n);
}

/**
 * Remove context from ready-list.
 */
IMPLEMENT inline NEEDS ["cpu_lock.h", <cassert>, "std_macros.h"]
template<typename E>
void
Ready_queue_wfq<E>::dequeue(E *i)
{
  assert (cpu_lock.test());

  // Don't dequeue threads which aren't enqueued
  if (EXPECT_FALSE (!i->in_ready_list() || i == idle))
    return;

  unsigned x = _e(i)->_ready_link - _heap;

  if (x == --_cnt)
    {
      _e(i)->_ready_link = 0;
      return;
    }

  swap(x, _cnt);
  heap_down(x);
  _e(i)->_ready_link = 0;
}

/**
 * Enqueue context in ready-list.
 */
PUBLIC
template<typename E>
void
Ready_queue_wfq<E>::requeue(E *i)
{
  if (!i->in_ready_list())
    enqueue(i, false);

  heap_down(_e(i)->_ready_link - _heap);
}


PUBLIC template< typename E > inline
void
Ready_queue_wfq<E>::deblock_refill(E *sc)
{
  Unsigned64 da = 0;
  if (EXPECT_TRUE(_current_sched != 0))
    da = _e(_current_sched)->_dl;

  if (_e(sc)->_dl >= da)
    return;

  _e(sc)->_left += (da - _e(sc)->_dl) * _e(sc)->_w;
  if (_e(sc)->_left > _e(sc)->_q)
    _e(sc)->_left = _e(sc)->_q;
  _e(sc)->_dl = da;
}

