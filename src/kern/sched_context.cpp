INTERFACE:
#include "per_cpu_data.h"

EXTENSION class Sched_context
{
public:
  class Ready_queue : public Ready_queue_base
  {
  public:
    void set_current_sched(Sched_context *sched);
    void invalidate_sched() { activate(0); }
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
 * \param sc Sched_context that shall be deblocked
 * \param crs the Sched_context of the currently running context
 * \param lazy_q queue lazily if applicable
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

      if ((EXPECT_TRUE(cs != 0) && cs->dominates(sc))
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
} __attribute__((packed));


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
              t,
              (unsigned)id, (unsigned)prio, left, quantum);
}


