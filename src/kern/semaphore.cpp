INTERFACE:

#include "irq.h"
#include "kobject_helper.h"
#include "prio_list.h"

class Semaphore : public Kobject_h<Semaphore, Irq>
{
public:
  friend class Jdb_kobject_irq;
  enum Op {
    Op_down = 0
  };

protected:
  Smword _queued;
  Locked_prio_list _waiting;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include "ipc_timeout.h"

JDB_DEFINE_TYPENAME(Semaphore,  "\033[37mIRQ sem\033[m");

PUBLIC explicit
Semaphore::Semaphore(Ram_quota *q = 0)
: Kobject_h<Semaphore, Irq>(q), _queued(0)
{
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
        ++_queued;
    }
  return old;
}

PRIVATE inline NOEXPORT
void ALWAYS_INLINE
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

  ui->ack();
  if (t)
    t->activate();
}

PRIVATE static
void
Semaphore::hit_edge_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Semaphore*>(i)->_hit_edge_irq(ui); }

PRIVATE inline NOEXPORT
void ALWAYS_INLINE
Semaphore::_hit_level_irq(Upstream_irq const *ui)
{
  assert (cpu_lock.test());
  mask_and_ack();
  ui->ack();
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
Semaphore::switch_mode(bool is_edge_triggered)
{
  hit_func = is_edge_triggered ? &hit_edge_irq : &hit_level_irq;
}

PUBLIC static inline
Sender *
Semaphore::sem_partner()
{ return reinterpret_cast<Sender *>(5); }

PRIVATE inline NOEXPORT
bool ALWAYS_INLINE
Semaphore::down(Thread *ct)
{
  bool run = true;
    {
      auto g = lock_guard(_waiting.lock());
      if (_queued == 0)
        {
          run = false;
          // set fake partner to avoid IPCs to the thread
          // TODO: make really sure that the partner pointer never gets
          //       dereferenced (use C++ types)
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

PRIVATE inline NOEXPORT
L4_msg_tag ALWAYS_INLINE
Semaphore::sys_down(L4_timeout t, Utcb const *utcb)
{
  Thread *const c_thread = ::current_thread();
  assert_opt (c_thread);

  enum
  {
    Thread_wait_mask = Thread_cancel | Thread_timeout
                       | Thread_receive_wait
  };

  down(c_thread);

  IPC_timeout timeout;

  if (!(c_thread->state() & Thread_ready))
    c_thread->setup_timer(t, utcb, &timeout);

  while (!(c_thread->state() & Thread_ready))
    c_thread->schedule();

  c_thread->reset_timeout();

  Mword s = c_thread->state();
  if (s & Thread_wait_mask)
    c_thread->state_del_dirty(Thread_wait_mask);

  if (EXPECT_FALSE(s & Thread_cancel))
    return commit_error(utcb, L4_error::R_canceled);

  if (EXPECT_FALSE(s & Thread_timeout))
    {
      if (c_thread->in_sender_list())
        {
          c_thread->set_partner(0);
          c_thread->set_wait_queue(0);
          c_thread->sender_dequeue(&_waiting);
        }
      return commit_error(utcb, L4_error::R_timeout);
    }

  return commit_result(0);
}

PUBLIC
L4_msg_tag
Semaphore::kinvoke(L4_obj_ref, L4_fpage::Rights /*rights*/, Syscall_frame *f,
                   Utcb const *utcb, Utcb *)
{
  L4_msg_tag tag = f->tag();

  if (EXPECT_FALSE(tag.words() < 1))
    return commit_result(-L4_err::EInval);

  Unsigned16 op = access_once(utcb->values) & 0xffff;

  switch (tag.proto())
    {
    case L4_msg_tag::Label_irq:
      return dispatch_irq_proto(op, _queued < 1);

    case L4_msg_tag::Label_semaphore:
      switch (op)
        {
        case Op_down:
          return sys_down(f->timeout().rcv, utcb);

        default:
          return commit_result(-L4_err::ENosys);
        }

    default:
      return commit_result(-L4_err::EBadproto);
    }
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
semaphore_factory(Ram_quota *q, Space *,
                  L4_msg_tag, Utcb const *,
                  int *err)
{
  static_assert(sizeof(Semaphore) <= sizeof(Irq_sender),
                "invalid allocator for semaphore used");
  *err = L4_err::ENomem;
  return Irq::allocate<Semaphore>(q);
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_semaphore, semaphore_factory);
}
}
