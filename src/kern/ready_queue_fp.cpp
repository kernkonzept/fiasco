INTERFACE [sched_fixed_prio || sched_fp_wfq]:

#include "config.h"
#include <cxx/dlist>
#include "member_offs.h"
#include "types.h"
#include "globals.h"


template<unsigned BITS>
class Hierarchical_bitmap
{
public:
  enum : unsigned
  {
    Root_bits = sizeof(unsigned) * 8,
    Num_words = (BITS + MWORD_BITS - 1U) / MWORD_BITS,
  };

  static_assert(Num_words <= Root_bits, "Depth of two is enough");

  void clear(unsigned long bit)
  {
    unsigned w = bit / MWORD_BITS;
    unsigned b = bit % MWORD_BITS;

    _leaves[w] &= ~(1UL << b);
    if (_leaves[w] == 0)
      _root &= ~(1U << w);
  }

  void set(unsigned long bit)
  {
    unsigned w = bit / MWORD_BITS;
    unsigned b = bit % MWORD_BITS;

    _leaves[w] |= (1UL << b);
    _root      |= (1U  << w);
  }

  unsigned find_highest_bit() const
  {
    if (!_root)
      return 0;

    unsigned w = Root_bits  - __builtin_clz(_root) - 1;
    unsigned b = MWORD_BITS - __builtin_clzl(_leaves[w]) - 1;

    return w * MWORD_BITS + b;
  }

private:
  unsigned _root = 0;
  Mword _leaves[Num_words] = { 0 };
};


struct L4_sched_param_fixed_prio : public L4_sched_param
{
  enum : Smword { Class = -1 };
  Mword quantum;
  unsigned short prio;
};

template<typename E>
class Ready_queue_fp
{
  friend class Jdb_thread_list;
  template<typename T>
  friend struct Jdb_thread_list_policy;
  friend class Sched_ctxts_test;
  friend class Scheduler_test;

private:
  typedef typename E::Fp_list List;

  unsigned prio_highest;
  Hierarchical_bitmap<256> prio_bmap;
  List prio_next[256];

public:
  void set_idle(E *sc)
  { sc->_prio = Config::Kernel_prio; }

  void enqueue(E *, bool);
  void dequeue(E *);
  E *next_to_run() const;
};


// ---------------------------------------------------------------------------
IMPLEMENTATION [sched_fixed_prio || sched_fp_wfq]:

#include <cassert>
#include "cpu_lock.h"
#include "std_macros.h"
#include "config.h"


IMPLEMENT inline
template<typename E>
E *
Ready_queue_fp<E>::next_to_run() const
{ return prio_next[prio_highest].front(); }

/**
 * Enqueue context in ready-list.
 */
IMPLEMENT
template<typename E>
void
Ready_queue_fp<E>::enqueue(E *i, bool is_current_sched)
{
  assert(cpu_lock.test());

  // Don't enqueue threads which are already enqueued
  if (EXPECT_FALSE (i->in_ready_list()))
    return;

  unsigned short prio = i->prio();

  if (prio > prio_highest)
    prio_highest = prio;

  prio_next[prio].push(i, is_current_sched ? List::Front : List::Back);
  prio_bmap.set(prio);
}

/**
 * Remove context from ready-list.
 */
IMPLEMENT inline NEEDS ["cpu_lock.h", <cassert>, "std_macros.h"]
template<typename E>
void
Ready_queue_fp<E>::dequeue(E *i)
{
  assert (cpu_lock.test());

  // Don't dequeue threads which aren't enqueued
  if (EXPECT_FALSE (!i->in_ready_list()))
    return;

  unsigned short prio = i->prio();

  prio_next[prio].remove(i);

  if (prio_next[prio].empty())
    {
      prio_bmap.clear(prio);
      prio_highest = prio_bmap.find_highest_bit();
    }
}


PUBLIC inline
template<typename E>
void
Ready_queue_fp<E>::requeue(E *i)
{
  if (!i->in_ready_list())
    enqueue(i, false);
  else
    prio_next[i->prio()].rotate_to(*++List::iter(i));
}


PUBLIC template<typename E> inline
void
Ready_queue_fp<E>::deblock_refill(E *)
{}

