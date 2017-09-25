
/*
 * Timeslice infrastructure
 */

INTERFACE[sched_wfq] :

#include "member_offs.h"
#include "types.h"
#include "globals.h"
#include "ready_queue_wfq.h"


class Sched_context : public Sched_context_wfq<Sched_context>
{
  MEMBER_OFFSET();
  friend class Jdb_list_timeouts;
  friend class Jdb_thread_list;

  template<typename T>
  friend class Jdb_thread_list_policy;
  friend class Sched_context_wfq<Sched_context>;

  union Sp
  {
    L4_sched_param p;
    L4_sched_param_wfq wfq;
  };

public:
  typedef Sched_context Wfq_sc;
  typedef Ready_queue_wfq<Sched_context> Ready_queue_base;
  Context *context() const { return context_of(this); }

private:
  static Sched_context *wfq_elem(Sched_context *x) { return x; }

  Sched_context **_ready_link;
  bool _idle:1;
  Unsigned64 _dl;
  Unsigned64 _left;

  unsigned _q;
  unsigned _w;
  unsigned _qdw;

  friend class Ready_queue_wfq<Sched_context>;
};

// --------------------------------------------------------------------------
IMPLEMENTATION [sched_wfq]:

#include <cassert>
#include "config.h"
#include "cpu_lock.h"
#include "std_macros.h"


/**
 * Constructor
 */
PUBLIC
Sched_context::Sched_context()
: _ready_link(0),
  _idle(0),
  _dl(0),
  _left(Config::Default_time_slice),
  _q(Config::Default_time_slice),
  _w(1),
  _qdw(_q / _w)
{}

/**
 * Return owner of Sched_context
 */
PUBLIC inline
Context *
Sched_context::owner() const
{
  return context();
}

PUBLIC
int
Sched_context::set(L4_sched_param const *_p)
{
  Sp const *p = reinterpret_cast<Sp const *>(_p);
  if (p->p.sched_class != L4_sched_param_wfq::Class)
    return -L4_err::ERange;

  if (p->wfq.quantum == 0 || p->wfq.weight == 0)
    return -L4_err::EInval;

  _dl = 0;
  _q = p->wfq.quantum;
  _w = p->wfq.weight;
  _qdw =  p->wfq.quantum / p->wfq.weight;
  return 0;
}

/**
 * Return remaining time quantum of Sched_context
 */
PUBLIC inline
Unsigned64
Sched_context::left() const
{
  return _left;
}

PUBLIC inline NEEDS[Sched_context::set_left]
void
Sched_context::replenish()
{
  set_left(_q);
  _dl += _qdw;
}

/**
 * Set remaining time quantum of Sched_context
 */
PUBLIC inline
void
Sched_context::set_left(Unsigned64 left)
{
  _left = left;
}

/**
 * Check if Context is in ready-list.
 * @return 1 if thread is in ready-list, 0 otherwise
 */
PUBLIC inline
Mword
Sched_context::in_ready_list() const
{
  return _ready_link != 0;
}

PUBLIC inline
bool
Sched_context::dominates(Sched_context *sc)
{
#if 0
  if (_idle)
    LOG_MSG_3VAL(current(), "idl", (Mword)sc, _dl, sc->_dl);
#endif
  return !_idle && _dl < sc->_dl;
}


PUBLIC static inline
unsigned
Sched_context::prio() { return 0; }

