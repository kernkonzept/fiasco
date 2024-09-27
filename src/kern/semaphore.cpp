INTERFACE:

#include <limits.h>
#include "irq.h"
#include "kobject_helper.h"
#include "prio_list.h"

class Semaphore : public Kobject_h<Semaphore, Irq>
{
  Semaphore() = delete;

public:
  friend class Jdb_kobject_irq;
  enum class Op : Mword
  {
    Down = 0
  };

protected:
  Smword _queued;
  Locked_prio_list _waiting;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include "ipc_timeout.h"
#include "assert_opt.h"

JDB_DEFINE_TYPENAME(Semaphore,  "\033[37mIRQ sem\033[m");

PUBLIC explicit __attribute__((nonnull))
Semaphore::Semaphore(Ram_quota *q)
: Kobject_h<Semaphore, Irq>(q), _queued(0)
{
  assert(q);
  hit_func = &hit_edge_irq;
  __unmask();
}

PRIVATE inline NOEXPORT
Smword
Semaphore::count_up(Thread **wakeup)
{
  Smword old;
    {
      auto g = lock_guard(_waiting.lock());
      old = _queued;
      Prio_list_elem *f = _waiting.first();
      if (f)
        {
          _waiting.dequeue(f);
          Thread *t = static_cast<Thread*>(Sender::cast(f));
          t->set_wait_queue(0);
          *wakeup = t;
        }
      else
	// avoid wrapping the _queued counter around to 0
	if (_queued < LONG_MAX)
	  ++_queued;
    }
  return old;
}

PRIVATE inline NOEXPORT ALWAYS_INLINE
void
Semaphore::_hit_edge_irq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());
  Thread *t = 0;
  Smword q = count_up(&t);

  // if we get a second edge triggered IRQ before the first is
  // handled we can mask the IRQ.  The consume function will
  // unmask the IRQ when the last IRQ is dequeued.
  if (q <= 0)
    ack();
  else
    mask_and_ack();

  Upstream_irq::ack(ui);
  if (t)
    t->activate();
}

PRIVATE static
void
Semaphore::hit_edge_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Semaphore*>(i)->_hit_edge_irq(ui); }

PRIVATE inline NOEXPORT ALWAYS_INLINE
void
Semaphore::_hit_level_irq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());
  mask_and_ack();
  Upstream_irq::ack(ui);
  Thread *t = 0;
  count_up(&t);

  if (t)
    t->activate();
}

PRIVATE static
void
Semaphore::hit_level_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Semaphore*>(i)->_hit_level_irq(ui); }


PUBLIC
void
Semaphore::switch_mode(bool is_edge_triggered) override
{
  hit_func = is_edge_triggered ? &hit_edge_irq : &hit_level_irq;
}

PUBLIC static inline
Sender *
Semaphore::sem_partner()
{ return reinterpret_cast<Sender *>(5); }

PRIVATE inline NOEXPORT ALWAYS_INLINE
bool
Semaphore::down(Thread *ct)
{
  bool run = true;
    {
      auto g = lock_guard(_waiting.lock());
      if (_queued == 0)
        {
          // This check is necessary to ensure that a thread does not enqueue
          // itself into the semaphore´s waiting queue after the semaphore
          // emptied its waiting queue and unblocked the waiters. Because then
          // the thread would block forever (assuming it specified an infinite
          // timeout).
          if (!existence_lock.valid())
            return true;

          run = false;
          // set fake partner to avoid IPCs to the thread
          ct->set_partner(sem_partner());
          ct->state_change_dirty(~Thread_ready, Thread_receive_wait);
          ct->set_wait_queue(&_waiting);
          ct->sender_enqueue(&_waiting, ct->sched()->prio());
        }
      else
        --_queued;
    }

   // auto unmask edge triggered IRQs if there is just one pending IRQ left
   if (access_once(&_queued) == 1 && hit_func == &hit_edge_irq)
     unmask();

   return run;
}

PRIVATE inline NEEDS["assert_opt.h"] NOEXPORT ALWAYS_INLINE
L4_msg_tag
Semaphore::sys_down(L4_fpage::Rights rights, L4_timeout t, Utcb const *utcb)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  Thread *const c_thread = ::current_thread();
  assert_opt (c_thread);

  enum
  {
    Thread_wait_mask = Thread_cancel | Thread_timeout
                       | Thread_receive_wait
  };

  // Reset IPC error -- Semaphore::destroy() could set it.
  const_cast<Utcb*>(utcb)->error = L4_error::None;

  down(c_thread);

  IPC_timeout timeout;

  if (!(c_thread->state() & Thread_ready))
    c_thread->setup_timer(t, utcb, &timeout);

  while (!(c_thread->state() & Thread_ready))
    c_thread->schedule();

  c_thread->reset_timeout();

  Mword s = c_thread->state();
  if (s & Thread_wait_mask)
    {
      c_thread->state_del_dirty(Thread_wait_mask);

      // Reset partner only after clearing the Thread_receive_wait flag, otherwise
      // the thread can be misperceived as being in an IPC open wait.
      c_thread->set_partner(nullptr);
    }

  if (EXPECT_FALSE(s & (Thread_cancel | Thread_timeout)))
    {
      if (c_thread->wait_queue())
        {
          // While waiting we released the CPU lock, therefore, we would
          // normally have to assume that our ephemeral reference to the
          // semaphore may have become invalid (semaphore has been deleted).
          // However, if the current thread is still in the semaphore's waiting
          // queue we know that our ephemeral reference to the semaphore object
          // is still valid.
          // The reason for this is that an RCU grace period must elapse between
          // Semaphore::destroy(), which dequeues all waiting threads, and the
          // subsequent deletion of the semaphore. Since an RCU grace period
          // acts as a full memory barrier (on any CPU that has passed the grace
          // period) and the grace period only begins after Semaphore::destroy(),
          // when seeing that the current thread is still in the wait queue,
          // we can be sure that an eventual semaphore destruction has not yet
          // passed the RCU wait and will not pass it until we release the CPU
          // lock the next time.
          // Thus it is safe to access the semaphore's waiting queue here.
          auto g = lock_guard(_waiting.lock());

          // Recheck under lock whether thread is still in semaphore's waiting
          // queue.
          if (c_thread->wait_queue())
            {
              c_thread->set_wait_queue(0);
              c_thread->sender_dequeue(&_waiting);
              return commit_error(utcb, (s & Thread_cancel) ? L4_error::R_canceled
                                                            : L4_error::R_timeout);
            }
          // else fall-through (cancel/timeout raced with unblock)
        }

      // While our wait was cancelled or timed out, Semaphore::count_up()
      // completed the down operation, i.e. successfully dequeued the thread
      // from the wait queue, unblocked it and adjusted the semaphore counter.
      // Therefore, the semaphore down operation must be reported as successful!
      // Also Semaphore::destroy() could be the reason why the thread was
      // dequeued and unblocked.
    }

  // An IPC error might have been set by Semaphore::destroy() to indicate that
  // the reason for unblocking the waiting thread was not the completion of the
  // down operation, but the destruction of the semaphore.
  if (utcb->error.ok())
    return commit_result(0);
  else
    return L4_msg_tag(0, 0, L4_msg_tag::Error, 0);
}

PUBLIC
L4_msg_tag
Semaphore::kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                   Utcb const *utcb, Utcb *)
{
  L4_msg_tag tag = f->tag();
  Mword op = get_irq_opcode(tag, utcb);

  if (EXPECT_FALSE(op == ~0UL))
    return commit_result(-L4_err::EInval);

  switch (tag.proto())
    {
    case L4_msg_tag::Label_irq:
      return dispatch_irq_proto(Irq::Op{op}, _queued < 1);

    case L4_msg_tag::Label_semaphore:
      switch (Op{op})
        {
        case Op::Down:
          return sys_down(rights, f->timeout().rcv, utcb);

        default:
          return commit_result(-L4_err::ENosys);
        }

    default:
      return commit_result(-L4_err::EBadproto);
    }
}

PUBLIC
void
Semaphore::destroy(Kobjects_list &reap_list) override
{
  Irq::destroy(reap_list);

  for (;;)
    {
      Thread *t;
        {
          auto g = lock_guard(_waiting.lock());
          Prio_list_elem *f = _waiting.first();
          if (!f)
            break;

          _waiting.dequeue(f);
          t = static_cast<Thread*>(Sender::cast(f));
          // Do not reset t´s partner because t still has Thread_receive_wait
          // set. The fake partner avoids IPCs to that thread.
          t->set_wait_queue(0);
          t->utcb().access(true)->error = L4_error::Not_existent;
        }

      t->activate();
    }
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
semaphore_factory(Ram_quota *q, Space *,
                  L4_msg_tag, Utcb const *, Utcb *,
                  int *err, unsigned *)
{
  static_assert(sizeof(Semaphore) <= sizeof(Irq_sender),
                "invalid allocator for semaphore used");
  *err = L4_err::ENomem;
  return Irq::allocate<Semaphore>(q);
}

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(semaphore_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_semaphore, semaphore_factory);
}

}
