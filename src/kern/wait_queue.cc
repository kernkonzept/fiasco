#include "wait_queue.h"

#include "entry_frame.h"
#include "ipc_timeout.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "ram_quota.h"
#include "sender.h"
#include "thread.h"
#include "thread_state.h"
#include "system_clock.h"

JDB_DEFINE_TYPENAME(Wait_queue, "\033[37mWQ\033[m");

static Kmem_slab_t<Wait_queue> _wq_allocator("Wait_queue");

using Ipc_flags = Sender::Ipc_flags;

// _wait_q holds either blocked senders or blocked receivers — never both.
// _has_senders is updated atomically under the _wait_q lock.

inline Thread *
Wait_queue::dequeue_receiver()
{
  auto g = lock_guard(_wait_q.qlock());
  if (_has_senders)
    return nullptr;
  return static_cast<Thread*>(Sender::cast(_wait_q.dequeue_first_dirty()));
}

inline Sender *
Wait_queue::dequeue_sender()
{
  auto g = lock_guard(_wait_q.qlock());
  if (!_has_senders)
    return nullptr;
  return Sender::cast(_wait_q.dequeue_first_dirty());
}

inline Thread *
Wait_queue::enqueue_sender(Thread *ct, unsigned short prio)
{
  auto g = lock_guard(_wait_q.qlock());
  if (!_has_senders && !_wait_q.empty())
    return static_cast<Thread*>(Sender::cast(_wait_q.dequeue_first_dirty()));

  _has_senders = true;
  _wait_q.insert_dirty(ct->qitem(), prio);
  return nullptr;
}

inline Sender *
Wait_queue::enqueue_receiver(Thread *ct, unsigned short prio)
{
  auto g = lock_guard(_wait_q.qlock());
  if (_has_senders && !_wait_q.empty())
    return Sender::cast(_wait_q.dequeue_first_dirty());

  _has_senders = false;
  _wait_q.insert_dirty(ct->qitem(), prio);
  return nullptr;
}

static inline L4_error
cancel_or_timeout(Mword state, L4_error::Phase phase)
{
  return (state & Thread_cancel)
    ? L4_error(L4_error::Canceled, phase)
    : L4_error(L4_error::Timeout,  phase);
}

/**
 * Set up an IPC timeout. Returns false if the timeout is zero or already
 * expired (caller must return immediately with a timeout error).
 */
static inline bool
setup_timeout(Thread *ct, L4_timeout to, Utcb *utcb, IPC_timeout *timeout)
{
  if (to.is_zero())
    return false;

  if (to.is_finite())
    {
      Unsigned64 system_clock = System_clock::clock();
      Unsigned64 tval = to.microsecs(system_clock, utcb);
      if (tval <= system_clock)
        return false;
      ct->set_timeout(timeout, tval);
    }
  return true;
}

void
Wait_queue::destroy(Kobject ***reap_list)
{
  Kobject::destroy(reap_list);
  auto g = lock_guard(cpu_lock);
  if (_has_senders)
    {
      while (auto *t = dequeue_sender())
        {
          t->ipc_receiver_aborted();
          Proc::preemption_point();
        }
    }
  else
    {
      while (Thread *t = dequeue_receiver())
        {
          t->utcb().access()->error = L4_error::Not_existent;
          t->activate();
        }
    }
}

inline void
Wait_queue::do_receive(Thread *ct, Syscall_frame *f, Utcb *utcb)
{
  auto *sender = dequeue_sender();
  if (!sender)
    {
      ct->set_rcv_regs(Ipc_flags(true, true), f);
      ct->sender_list()->reset_poi(reinterpret_cast<Address>(this));
      sender = enqueue_receiver(ct, ct->sched_context()->prio());
    }

  if (sender)
    {
      ct->prepare_receive(Ipc_flags(true, true), sender, f);
      ct->state.add(Thread_receive_in_progress);
      sender->ipc_send_msg(ct, ct);
      ct->state.del(Thread_ipc_mask);
      return;
    }

  IPC_timeout timeout;
  if (!setup_timeout(ct, f->timeout().rcv, utcb, &timeout))
    {
      _wait_q.dequeue(ct->qitem());
      ct->sender_list()->reset_poi();
      f->tag(commit_error(utcb, L4_error(L4_error::Timeout, L4_error::Rcv)));
      return;
    }

  ct->state.change(~Thread_ready, Thread_receive_wait);

  Mword state;
  while (!((state = ct->state()) & Thread_ready))
    ct->schedule();

  ct->sender_list()->reset_poi();
  ct->reset_timeout();

  if (state & (Thread_cancel | Thread_timeout | Thread_receive_wait))
    {
      ct->state.del(Thread_full_ipc_mask);
      _wait_q.dequeue(ct->qitem());
      f->tag(commit_error(utcb, cancel_or_timeout(state, L4_error::Rcv)));
    }
}

void
Wait_queue::do_send_ipc(Thread *ct, L4_obj_ref self,
                        Syscall_frame *f, Utcb *utcb)
{
  bool have_recv = self.have_recv();

  Thread *receiver = dequeue_receiver();

  if (!receiver)
    {
      ct->set_snd_msg_tag(f->tag());
      ct->set_rcv_regs(Ipc_flags(have_recv, false), f);
      if (have_recv)
        ct->sender_list()->reset_poi(reinterpret_cast<Address>(this));

      receiver = enqueue_sender(ct, ct->sched_context()->prio());
    }

  if (receiver)
    {
      receiver->set_partner(ct);
      ct->do_ipc(f->tag(), receiver, Ipc_flags(have_recv, false),
                 have_recv ? receiver : nullptr,
                 f->timeout(), f);
      return;
    }

  ct->state.change(~Thread_ready, Thread_send_wait);
  IPC_timeout timeout;
  if (!setup_timeout(ct, f->timeout().snd, utcb, &timeout))
    {
      ct->state.del(Thread_full_ipc_mask);
      _wait_q.dequeue(ct->qitem());
      f->tag(commit_error(utcb, L4_error(L4_error::Timeout, L4_error::Snd)));
      return;
    }

  Mword ipc_state;
  auto test_state = [&ipc_state](Mword s)
    {
      ipc_state = s & (Thread_send_wait | Thread_ipc_abort_mask);
      return (ipc_state == Thread_send_wait) || (s & Thread_send_in_progress);
    };

  while (ct->state.change_if(test_state, ~Thread_ready, 0))
    ct->schedule();

  ct->reset_timeout();
  _wait_q.dequeue(ct->qitem());

  if (EXPECT_FALSE(ipc_state != 0))
    {
      ct->state.del(Thread_full_ipc_mask);
      if (ipc_state & (Thread_timeout | Thread_cancel))
        f->tag(commit_error(utcb, cancel_or_timeout(ipc_state, L4_error::Snd)));
      else
        f->tag(L4_msg_tag::error());
      return;
    }

  if (!have_recv || !(ct->state() & Thread_ipc_receive_mask))
    return;

  Mword rcv_state;
  auto test_waiting = [&rcv_state](Mword s) -> bool
    {
      rcv_state = s & Thread_ipc_abort_mask;
      if (rcv_state)
        return false;
      return s & Thread_ipc_receive_mask;
    };

  if (ct->state.change_if(test_waiting, ~Thread_ready, 0))
    ct->schedule();

  if (EXPECT_TRUE(rcv_state == 0))
    return;

  ct->state.del(Thread_full_ipc_mask);
  if (rcv_state & (Thread_timeout | Thread_cancel))
    f->tag(commit_error(utcb, cancel_or_timeout(rcv_state, L4_error::Rcv)));
  else
    f->tag(L4_msg_tag::error());
}

void
Wait_queue::invoke(L4_obj_ref self, L4_fpage::Rights,
                   Syscall_frame *f, Utcb *utcb)
{
  Thread *ct = current_thread();
  bool have_recv = self.have_recv();

  // --- Receive: receiver waits for a sender ---
  if (have_recv && !(self.op() & L4_obj_ref::Ipc_send))
    {
      do_receive(ct, f, utcb);
      return;
    }

  // --- Reply-and-wait: reply via reply cap, then wait on WQ ---
  if ((self.op() & (L4_obj_ref::Ipc_send | L4_obj_ref::Ipc_reply))
      == (L4_obj_ref::Ipc_send | L4_obj_ref::Ipc_reply))
    {
      Context::Reply_cap reply_cap = ct->reply_cap();
      Thread *reply_target = static_cast<Thread*>(reply_cap.receiver());
      if (reply_target)
        {
          ct->set_ipc_from_spec(f->from_spec(), reply_cap.rights(), true);
          ct->do_ipc(f->tag(), reply_target, Ipc_flags{}, nullptr, f->timeout(), f);
        }

      do_receive(ct, f, utcb);
      return;
    }

  // --- Send (or call): sender delivers via the wait queue ---
  if (self.op() & L4_obj_ref::Ipc_send)
    {
      if (f->tag().proto() == L4_msg_tag::Label_wait_queue)
        {
          f->tag(commit_result(-L4_err::ENosys));
          return;
        }

      do_send_ipc(ct, self, f, utcb);
      return;
    }

  f->tag(commit_error(utcb, L4_error::Not_existent));
}

void
Wait_queue::operator delete (void *ptr)
{
  Wait_queue *wq = cxx::launder(static_cast<Wait_queue*>(ptr));
  Ram_quota *q = wq->_quota;
  _wq_allocator.free(wq);
  if (q)
    q->free(sizeof(Wait_queue));
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
wait_queue_factory(Ram_quota *q, Space *space,
                   L4_msg_tag, Utcb const *,
                   int *err)
{
  *err = L4_err::ENomem;
  return _wq_allocator.q_new(q, q, space);
}

static inline void __attribute__((constructor)) FIASCO_INIT_SFX(wait_queue_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_wait_queue, wait_queue_factory);
}
}
