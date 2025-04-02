INTERFACE:
#include "per_cpu_data.h"

EXTENSION class Sched_context
{
public:
  class Ready_queue : public Ready_queue_base
  {
  public:
    void set_current_sched(Sched_context *sched);
    void invalidate_sched() { activate(nullptr); }
    bool deblock(Sched_context *sc, Sched_context *crs, bool lazy_q = false);
    void ready_enqueue(Sched_context *sc)
    {
      assert(cpu_lock.test());

      // Don't enqueue threads which are already enqueued
      if (EXPECT_FALSE (sc->in_ready_list()))
        return;

      enqueue(sc, true);
    }

    void ready_dequeue(Sched_context *sc)
    {
      assert (cpu_lock.test());

      // Don't dequeue threads which aren't enqueued
      if (EXPECT_FALSE (!sc->in_ready_list()))
        return;

      dequeue(sc);
    }

    void switch_sched(Sched_context *from, Sched_context *to)
    {
      assert (cpu_lock.test());

      // If we're leaving the global timeslice, invalidate it This causes
      // schedule() to select a new timeslice via set_current_sched()
      if (from == current_sched())
        invalidate_sched();

      if (from->in_ready_list())
        dequeue(from);

      enqueue(to, false);
    }

    Context *schedule_in_progress;
  };

  static Per_cpu<Ready_queue> rq;
};

IMPLEMENTATION:

#include <cassert>
#include "timer.h"
#include "timeout.h"
#include "globals.h"
#include "logdefs.h"

DEFINE_PER_CPU Per_cpu<Sched_context::Ready_queue> Sched_context::rq;

/**
 * Set currently active global Sched_context.
 */
IMPLEMENT
void
Sched_context::Ready_queue::set_current_sched(Sched_context *sched)
{
  assert (sched);
  // Save remainder of previous timeslice or refresh it, unless it had
  // been invalidated
  Timeout * const tt = timeslice_timeout.current();
  Unsigned64 clock = Timer::system_clock();
  if (Sched_context *s = current_sched())
    {
      Signed64 left = tt->get_timeout(clock);
      if (left > 0)
        s->set_left(left);
      else
        s->replenish();

      LOG_SCHED_SAVE(s);
    }

  // Program new end-of-timeslice timeout
  tt->reset();
  tt->set(clock + sched->left(), current_cpu());

  // Make this timeslice current
  activate(sched);

  LOG_SCHED_LOAD(sched);
}


/**
 * Deblock the given scheduling context, i.e. add the scheduling context to the
 * ready queue.
 *
 * As an optimization, if requested by setting the `lazy_q` parameter, only adds
 * the deblocked scheduling context to the ready queue if it cannot preempt the
 * currently active scheduling context, i.e. rescheduling is not necessary.
 * Otherwise the caller is responsible to switch to the lazily deblocked
 * scheduling context via `switch_to_locked()`. This is required to ensure that
 * the scheduler does not forget about the scheduling context.
 *
 * \param sc      Sched_context that shall be deblocked.
 * \param crs     Sched_context of the currently running context.
 * \param lazy_q  Queue lazily if applicable.
 *
 * \returns Whether a reschedule is necessary (deblocked scheduling context
 *          can preempt the currently running scheduling context).
 */
IMPLEMENT inline NEEDS[<cassert>]
bool
Sched_context::Ready_queue::deblock(Sched_context *sc, Sched_context *crs, bool lazy_q)
{
  assert(cpu_lock.test());

  Sched_context *cs = current_sched();
  bool res = true;
  if (sc == cs)
    {
      if (crs && crs->dominates(sc))
        res = false;
    }
  else
    {
      deblock_refill(sc);

      if ((EXPECT_TRUE(cs != nullptr) && cs->dominates(sc))
          || (crs && crs->dominates(sc)))
        res = false;
    }

  if (res && lazy_q)
    return true;

  ready_enqueue(sc);
  return res;
}

INTERFACE [debug]:

#include "tb_entry.h"

/** logged scheduling event. */
class Tb_entry_sched : public Tb_entry
{
public:
  unsigned short mode;
  Context const *owner;
  unsigned short id;
  unsigned short prio;
  signed long left;
  unsigned long quantum;

  void print(String_buffer *buf) const;
};
static_assert(sizeof(Tb_entry_sched) <= Tb_entry::Tb_entry_size);


IMPLEMENTATION [debug]:

#include "kobject_dbg.h"
#include "string_buffer.h"

// sched
IMPLEMENT void
Tb_entry_sched::print(String_buffer *buf) const
{
  Mword t = Kobject_dbg::pointer_to_id(owner);

  buf->printf("(ts %s) owner:%lx id:%2x, prio:%2x, left:%6ld/%-6lu",
              mode == 0 ? "save" :
              mode == 1 ? "load" :
              mode == 2 ? "invl" : "????",
              t, id, prio, left, quantum);
}


