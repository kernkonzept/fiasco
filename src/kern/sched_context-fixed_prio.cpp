
/*
 * Timeslice infrastructure
 */

INTERFACE [sched_fixed_prio]:

#include <cxx/dlist>
#include "member_offs.h"
#include "types.h"
#include "globals.h"
#include "ready_queue_fp.h"


class Sched_context : public cxx::D_list_item
{
  MEMBER_OFFSET();
  friend class Jdb_list_timeouts;
  friend class Jdb_thread_list;

  template<typename T>
  friend struct Jdb_thread_list_policy;

  union Sp
  {
    L4_sched_param p;
    L4_sched_param_legacy legacy_fixed_prio;
    L4_sched_param_fixed_prio fixed_prio;
  };

public:
  typedef cxx::Sd_list<Sched_context> Fp_list;

  class Ready_queue_base : public Ready_queue_fp<Sched_context>
  {
  public:
    void activate(Sched_context *s)
    { _current_sched = s; }
    Sched_context *current_sched() const { return _current_sched; }
    void ready_enqueue(Sched_context *sc)
    {
      assert(cpu_lock.test());

      // Don't enqueue threads which are already enqueued
      if (EXPECT_FALSE (sc->in_ready_list()))
        return;

      enqueue(sc, sc == current_sched());
    }

  private:
    Sched_context *_current_sched;
  };

  Context *context() const { return context_of(this); }

private:
  unsigned short _prio;
  Unsigned64 _quantum;
  Unsigned64 _left;

  friend class Ready_queue_fp<Sched_context>;
};


IMPLEMENTATION [sched_fixed_prio]:

#include <cassert>
#include "cpu_lock.h"
#include "std_macros.h"
#include "config.h"


/**
 * Constructor
 */
PUBLIC
Sched_context::Sched_context()
: _prio(Config::Default_prio),
  _quantum(Config::Default_time_slice),
  _left(Config::Default_time_slice)
{}


/**
 * Return priority of Sched_context
 */
PUBLIC inline
unsigned short
Sched_context::prio() const
{
  return _prio;
}

PUBLIC
int
Sched_context::set(L4_sched_param const *_p)
{
  Sp const *p = reinterpret_cast<Sp const *>(_p);
  if (p->p.sched_class >= 0)
    {
      // legacy fixed prio
      _prio = p->legacy_fixed_prio.prio;
      if (p->legacy_fixed_prio.prio > 255)
        _prio = 255;

      _quantum = p->legacy_fixed_prio.quantum;
      if (p->legacy_fixed_prio.quantum == 0)
        _quantum = Config::Default_time_slice;
      return 0;
    }

  switch (p->p.sched_class)
    {
    case L4_sched_param_fixed_prio::Class:
      _prio = p->fixed_prio.prio;
      if (p->fixed_prio.prio > 255)
        _prio = 255;

      _quantum = p->fixed_prio.quantum;
      if (p->fixed_prio.quantum == 0)
        _quantum = Config::Default_time_slice;
      break;

    default:
      return L4_err::ERange;
    };
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
{ set_left(_quantum); }

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
  return Fp_list::in_list(this);
}

PUBLIC inline
bool
Sched_context::dominates(Sched_context *sc)
{ return prio() > sc->prio(); }

