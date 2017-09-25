INTERFACE [sched_fp_wfq]:

#include "ready_queue_fp.h"
#include "ready_queue_wfq.h"

class Sched_context
{
  MEMBER_OFFSET();
  friend class Jdb_list_timeouts;
  friend class Jdb_thread_list;
  friend class Ready_queue_wfq<Sched_context>;

  union Sp
  {
    L4_sched_param p;
    L4_sched_param_legacy legacy_fixed_prio;
    L4_sched_param_fixed_prio fixed_prio;
    L4_sched_param_wfq wfq;
  };

  struct Ready_list_item_concept
  {
    typedef Sched_context Item;
    static Sched_context *&next(Sched_context *e) { return e->_sc.fp._ready_next; }
    static Sched_context *&prev(Sched_context *e) { return e->_sc.fp._ready_prev; }
    static Sched_context const *next(Sched_context const *e)
    { return e->_sc.fp._ready_next; }
    static Sched_context const *prev(Sched_context const *e)
    { return e->_sc.fp._ready_prev; }
  };

public:
  enum Type { Fixed_prio, Wfq };

  typedef cxx::Sd_list<Sched_context, Ready_list_item_concept> Fp_list;

private:
  Type _t;

  struct B_sc
  {
    unsigned short _p;
    unsigned _q;
    Unsigned64 _left;

    unsigned prio() const { return _p; }
  };


  struct Fp_sc : public B_sc
  {
    Sched_context *_ready_next, *_ready_prev;
  };

  struct Wfq_sc : public Sched_context_wfq<Wfq_sc>, public B_sc
  {
    Sched_context **_ready_link;
    bool _idle:1;
    Unsigned64 _dl;

    unsigned _w;
    unsigned _qdw;
  };

  union Sc
  {
    Wfq_sc wfq;
    Fp_sc fp;
  };

  Sc _sc;

public:
  static Wfq_sc *wfq_elem(Sched_context *x) { return &x->_sc.wfq; }

  struct Ready_queue_base
  {
  public:
    Ready_queue_fp<Sched_context> fp_rq;
    Ready_queue_wfq<Sched_context> wfq_rq;
    Sched_context *current_sched() const { return _current_sched; }
    void activate(Sched_context *s)
    {
      if (!s || s->_t == Wfq)
	wfq_rq.activate(s);
      _current_sched = s;
    }

    void enqueue(Sched_context *sc, bool is_current);
    void dequeue(Sched_context *);
    void requeue(Sched_context *sc);

    void set_idle(Sched_context *sc)
    { sc->_t = Wfq; sc->_sc.wfq._p = 0; wfq_rq.set_idle(sc); }

    Sched_context *next_to_run() const;
    void deblock_refill(Sched_context *sc);

  private:
    friend class Jdb_thread_list;
    Sched_context *_current_sched;
  };

  Context *context() const { return context_of(this); }
};


IMPLEMENTATION [sched_fp_wfq]:

#include <cassert>
#include "cpu_lock.h"
#include "std_macros.h"
#include "config.h"

/**
 * Constructor
 */
PUBLIC
Sched_context::Sched_context()
{
  _t = Fixed_prio;
  _sc.fp._p = Config::Default_prio;
  _sc.fp._q = Config::Default_time_slice;
  _sc.fp._left = Config::Default_time_slice;
  _sc.fp._ready_next = 0;
}

IMPLEMENT inline
Sched_context *
Sched_context::Ready_queue_base::next_to_run() const
{
  Sched_context *s = fp_rq.next_to_run();
  if (s)
    return s;

  return wfq_rq.next_to_run();
}

/**
 * Check if Context is in ready-list.
 * @return 1 if thread is in ready-list, 0 otherwise
 */
PUBLIC inline
Mword
Sched_context::in_ready_list() const
{
  // this magically works for the fp list and the heap,
  // because wfq._ready_link and fp._ready_next are the
  // same memory location
  return _sc.wfq._ready_link != 0;
}

PUBLIC inline
unsigned
Sched_context::prio() const
{ return _sc.fp._p; }

PUBLIC
int
Sched_context::set(L4_sched_param const *_p)
{
  Sp const *p = reinterpret_cast<Sp const *>(_p);
  if (p->p.sched_class >= 0)
    {
      // legacy fixed prio
      _t = Fixed_prio;
      _sc.fp._p = p->legacy_fixed_prio.prio;
      if (p->legacy_fixed_prio.prio > 255)
        _sc.fp._p = 255;

      _sc.fp._q = p->legacy_fixed_prio.quantum;
      if (p->legacy_fixed_prio.quantum == 0)
        _sc.fp._q = Config::Default_time_slice;
      return 0;
    }
  switch (p->p.sched_class)
    {
    case L4_sched_param_fixed_prio::Class:
      _t = Fixed_prio;

      _sc.fp._p = p->fixed_prio.prio;
      if (p->fixed_prio.prio > 255)
        _sc.fp._p = 255;

      _sc.fp._q = p->fixed_prio.quantum;
      if (p->fixed_prio.quantum == 0)
        _sc.fp._q = Config::Default_time_slice;

      break;
    case L4_sched_param_wfq::Class:
      if (p->wfq.quantum == 0 || p->wfq.weight == 0)
        return -L4_err::EInval;
      _t = Wfq;
      _sc.wfq._p = 0;
      _sc.wfq._q = p->wfq.quantum;
      _sc.wfq._w = p->wfq.weight;
      _sc.wfq._qdw =  p->wfq.quantum / p->wfq.weight;
      break;
    default:
      return L4_err::ERange;
    };
  return 0;
}


IMPLEMENT inline
void
Sched_context::Ready_queue_base::deblock_refill(Sched_context *sc)
{
  if (sc->_t != Wfq)
    fp_rq.deblock_refill(sc);
  else
    wfq_rq.deblock_refill(sc);
}

/**
 * Enqueue context in ready-list.
 */
IMPLEMENT
void
Sched_context::Ready_queue_base::enqueue(Sched_context *sc, bool is_current)
{
  if (sc->_t == Fixed_prio)
    fp_rq.enqueue(sc, is_current);
  else
    wfq_rq.enqueue(sc, is_current);
}

/**
 * Remove context from ready-list.
 */
IMPLEMENT inline NEEDS ["cpu_lock.h", "std_macros.h"]
void
Sched_context::Ready_queue_base::dequeue(Sched_context *sc)
{
  if (sc->_t == Fixed_prio)
    fp_rq.dequeue(sc);
  else
    wfq_rq.dequeue(sc);
}

IMPLEMENT
void
Sched_context::Ready_queue_base::requeue(Sched_context *sc)
{
  if (sc->_t == Fixed_prio)
    fp_rq.requeue(sc);
  else
    wfq_rq.requeue(sc);
}

PUBLIC inline
bool
Sched_context::dominates(Sched_context *sc)
{
  if (_t == Fixed_prio)
    return prio() > sc->prio();

  if (_sc.wfq._idle)
    return false;

  if (sc->_t == Fixed_prio)
    return false;

  return _sc.wfq._dl < sc->_sc.wfq._dl;
}

PUBLIC inline
void
Sched_context::replenish()
{
  _sc.fp._left = _sc.fp._q;
  if (_t == Wfq)
    _sc.wfq._dl += _sc.wfq._qdw;
}


PUBLIC inline
void
Sched_context::set_left(Unsigned64 l)
{ _sc.fp._left = l; }

PUBLIC inline
Unsigned64
Sched_context::left() const
{ return _sc.fp._left; }

