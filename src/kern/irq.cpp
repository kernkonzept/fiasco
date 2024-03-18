INTERFACE:

#include "ipc_sender.h"
#include "irq_chip.h"
#include "kobject_helper.h"
#include "member_offs.h"
#include "sender.h"
#include "context.h"

class Ram_quota;
class Thread;


/** Hardware interrupts.  This class encapsulates hardware IRQs.  Also,
    it provides a registry that ensures that only one receiver can sign up
    to receive interrupt IPC messages.
 */
class Irq : public Irq_base, public cxx::Dyn_castable<Irq, Kobject>
{
  MEMBER_OFFSET();
  Irq() = delete;

public:
  enum Op
  {
    Op_trigger    = 2, // Irq_sender + Irq_semaphore
    Op_eoi        = 4, // Icu + Irq_sender + Irq_semaphore
  };

protected:
  Ram_quota *_q;
};


/**
 * IRQ kernel object to send IPC messages to a receiving thread.
 *
 * When an IRQ hits, the Irq_sender queues itself to the thread it is bound to
 * (target thread). When no thread is bound to the Irq_sender, the IRQ is
 * dropped.
 *
 * To enqueue the Irq_sender at the thread, the Irq_sender sends a DRQ to the
 * target thread (asynchronously). We ensure that the Irq_sender queues its
 * DRQ request to at most one thread at a time by calling send() only if
 * queued() was previously zero. The _queued counter is only reset by Ipc_sender
 * via Irq_sender::re/deqeue_sender().
 *
 * When the Irq_sender is rebound to a different thread, it dequeues itself from
 * the old target thread (via Remote::abort_send()) and then re-injects the IRQ
 * to the new thread if it was pending.
 * If the target thread changes between an IPC send operation and the processing
 * of that send operation in the target thread, the old target thread forwards
 * the IPC send operation to the new target thread.
 */
class Irq_sender
: public Kobject_h<Irq_sender, Irq>,
  public Ipc_sender<Irq_sender>,
  public Ref_cnt_obj
{
  friend struct Irq_sender_test;

public:
  enum Op {
    Op_attach = 0,
    Op_detach = 1,
    Op_bind     = 0x10,
  };

protected:
  static bool is_valid_thread(Thread const *t)
  { return t != nullptr; }

  Smword _queued;
  Thread *_irq_thread;

private:
  Mword _irq_id;
  // Must only be used for sending async DRQs (no answer), because for regular
  // DRQs the DRQ reply is sent to Drq::context(), which assumes that the Drq
  // object is member of a Context.
  Context::Drq _drq;
};


//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include "atomic.h"
#include "config.h"
#include "cpu.h"
#include "cpu_lock.h"
#include "entry_frame.h"
#include "globals.h"
#include "ipc_sender.h"
#include "kmem_slab.h"
#include "kobject_rpc.h"
#include "lock_guard.h"
#include "minmax.h"
#include "std_macros.h"
#include "thread_object.h"
#include "thread_state.h"
#include "l4_buf_iter.h"
#include "vkey.h"
#include "global_data.h"

JDB_DEFINE_TYPENAME(Irq_sender, "\033[37mIRQ ipc\033[m");

namespace {
static Irq_base *irq_base_dcast(Kobject_iface *o)
{ return cxx::dyn_cast<Irq*>(o); }

struct Irq_base_cast
{
  Irq_base_cast()
  { Irq_base::dcast = &irq_base_dcast; }
};

static Irq_base_cast register_irq_base_cast;
}

PROTECTED inline
int
Irq::get_irq_opcode(L4_msg_tag tag, Utcb const *utcb)
{
  if (tag.proto() == L4_msg_tag::Label_irq && tag.words() == 0)
    return Op_trigger;
  if (EXPECT_FALSE(tag.words() < 1))
    return -1;

  return access_once(utcb->values) & 0xffff;
}

PROTECTED inline
L4_msg_tag
Irq::dispatch_irq_proto(Unsigned16 op, bool may_unmask)
{
  switch (op)
    {
    case Op_eoi:
      if (may_unmask)
        unmask();
      return L4_msg_tag(L4_msg_tag::Schedule); // no reply

    case Op_trigger:
      log();
      hit(0);
      return L4_msg_tag(L4_msg_tag::Schedule); // no reply

    default:
      return commit_result(-L4_err::ENosys);
    }
}

PUBLIC virtual
bool
Irq_sender::put() override
{ return dec_ref() == 0; }

PRIVATE inline
bool
Irq_sender::is_destroyed()
{ return !existence_lock.valid(); }

/**
 * Set new target thread and label.
 *
 * \pre               The _irq_lock must be locked to guard against
 *                    concurrent reconfiguration.
 *
 * \param target      The receiver that wants to receive IPC messages for this
 *                    IRQ. Might be nullptr to detach.
 * \param irq_id      The label for the IPC send operation.
 */
PRIVATE inline
void
Irq_sender::set_irq_thread(Thread *target, Mword irq_id)
{
  assert(_irq_lock.is_locked());

  // note: this is a possible race on user-land where the label of an IRQ might
  // become inconsistent with the attached thread. The user is responsible to
  // synchronize Irq::attach calls to prevent this.
  _irq_id = irq_id;
  Mem::mp_wmb();  // pairs with read barrier in send_local()
  _irq_thread = target;

  if (!target)
    return;

  target->inc_ref();
  if (Cpu::online(target->home_cpu()))
    _chip->set_cpu(pin(), target->home_cpu());
}

/**
 * Replace old target thread with a new one (first phase, under lock).
 *
 * Takes care of exchanging the old target thread with the new target
 * thread and the given label.
 *
 * \pre               The _irq_lock must be locked to guard against
 *                    concurrent reconfiguration.
 *
 * \param target      The receiver that wants to receive IPC messages for this
 *                    IRQ. Might be nullptr to unbind.
 * \param irq_id      The label for the IPC send operation.
 *
 * \return Old target thread if any, it is the responsibility of the caller to
 *         invoke Irq_sender::finish_replace_irq_thread() on the old target
 *         thread (without releasing the CPU lock in between).
 */
PRIVATE inline
Thread *
Irq_sender::start_replace_irq_thread(Thread *target, Mword irq_id)
{
  assert(_irq_lock.is_locked());

  Thread *old = _irq_thread;
  set_irq_thread(target, irq_id);
  return old;
}

/**
 * Replace old target thread with a new one (second phase, not under lock).
 *
 * Takes care of revoking the IRQ from the old target thread and then
 * re-injecting the IRQ to the new thread if it was pending.
 *
 * \pre               The cpu_lock must be held, but no other locks, as this
 *                    function executes a DRQ, i.e. the operation might block.
 * \pre               Must be assumed to be a potential preemption point, the
 *                    caller has to ensure that Irq_sender object cannot be
 *                    deleted, for example by holding counted reference to it.
 *
 * \param old         Old target thread if any.
 *
 * \retval 0          On success, `target` is the new IRQ handler thread.
 *                    IPC was not pending.
 * \retval 1          On success, `target` is the new IRQ handler thread.
 *                    IPC was pending on old target thread.
 *
 * \post              As this function executes a DRQ, it must be assumed to be a
 *                    potential preemption point.
 */
PRIVATE
int
Irq_sender::finish_replace_irq_thread(Thread *old)
{
  if (!old)
    return 0;

  int result;
  switch (old->Receiver::abort_send(this))
    {
    case Receiver::Abt_ipc_done:
      result = 0;
      break; // was not queued

    case Receiver::Abt_ipc_cancel:
      result = 1; // was queued
      break;

    default:
      // this must not happen as this is only the case for IPC including
      // message items and an IRQ never sends message items.
      panic("IRQ IPC flagged as in progress");
    }

  if (old->dec_ref() == 0)
    delete old;

  // re-inject if IRQ was queued before
  if (result > 0)
    {
      // We might have been preempted during DRQ execution, i.e. the someone
      // might have re-bound to a different thread and might even have deleted
      // the old thread. We therefore have to re-read the currently bound thread
      // under the _irq_lock.
      Thread *target;
        {
          auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

          // Do not forward if the Irq_sender was destroyed in the meantime.
          if (is_destroyed())
            return 0;

          target = _irq_thread;
        }

      send(target);
    }

  return result;
}

/**
 * Bind a receiver to this device interrupt.
 *
 * \pre `cpu_lock` must be held (ephemeral reference to Irq_sender might get lost otherwise)
 *
 * \param target      Thread that wants to receive IPC messages for this IRQ.
 * \param utcb        The input UTCB.
 * \param utcb_out    The output UTCB.
 *
 * \retval 0          On success, `target` is the new IRQ handler thread.
 *
 * \retval L4_error::Not_existent  Irq_sender object was deleted
 *
 * \post This function must be assumed to be a potential preemption point (see
 *       finish_replace_irq_thread()). In particular that means the Irq_sender
 *       object might have been deleted, unless the caller holds a counted
 *       reference to it.
 */
PUBLIC
L4_msg_tag
Irq_sender::bind_irq_thread(Thread *target, Utcb const *utcb, Utcb *utcb_out)
{
  assert(cpu_lock.test());

  Mword irq_id = access_once(&utcb->values[1]);

  Thread *old;
    {
      // Grab the _irq_lock to guard against concurrent bind/unbinds.
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);
      // Prevent binding thread if the Irq is invalid.
      if (is_destroyed())
        return commit_error(utcb_out, L4_error::Not_existent);

      if (_irq_thread == target)
        {
          _irq_id = irq_id;
          return commit_result(0);
        }

      old = start_replace_irq_thread(target, irq_id);
    }

  // The following operations are a preemption point, so we have to hold a
  // reference to Irq_sender to ensure it is not deleted in the meantime.
  Ref_ptr self(this);
  finish_replace_irq_thread(old);
  return commit_result(0);
}

PUBLIC
Receiver *
Irq_sender::owner() const { return _irq_thread; }

/**
 * Release an interrupt.
 *
 * \pre The cpu_lock must be held, but no other locks, as this
 *      function executes a DRQ, i.e. the operation might block.
 *
 * \pre Must be assumed to be a potential preemption point, the caller has to
 *      ensure that Irq_sender object cannot be deleted, for example by holding
 *      counted reference to it.
 *
 * \retval 0  On success, interrupt was inactive.
 * \retval 1  On success, interrupt was pending.
 */
PRIVATE
int
Irq_sender::detach_irq_thread()
{
  Thread *old;
    {
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

      if (!_irq_thread)
        return -L4_err::ENoent;

      mask();

      old = start_replace_irq_thread(nullptr, ~0UL);
    }

  return finish_replace_irq_thread(old);
}

PUBLIC explicit
Irq_sender::Irq_sender(Ram_quota *q)
: Kobject_h<Irq_sender, Irq>(q), _queued(0), _irq_thread(nullptr), _irq_id(~0UL)
{
  // Capability reference (released when last capability to Irq_sender object is
  // dropped).
  inc_ref();
  hit_func = &hit_level_irq;
}

PUBLIC
Irq_sender::~Irq_sender()
{
  assert(!_irq_thread);
}

PUBLIC
void
Irq_sender::switch_mode(bool is_edge_triggered) override
{
  hit_func = is_edge_triggered ? &hit_edge_irq : &hit_level_irq;
}

PUBLIC
void
Irq_sender::destroy(Kobject ***rl) override
{
  auto g = lock_guard(cpu_lock);
  Irq::destroy(rl);
  // Must be done _after_ returning from Irq::destroy() to make sure that the
  // existence lock was finally released by the last owner (the existence lock
  // was already invalidated before) -- see also Irq_sender::bind_irq_thread().
  static_cast<void>(detach_irq_thread());
}


/** Consume all interrupts.
    @return number of IRQs that are still pending -- this is always 0.
 */
PRIVATE inline NEEDS ["atomic.h"]
Smword
Irq_sender::consume()
{
  Smword old;

  do
    {
      old = _queued;
    }
  while (!cas(&_queued, old, 0L));
  Mem::mp_acquire();

  if (old >= 2 && hit_func == &hit_edge_irq)
    unmask();

  return 0L;
}

PUBLIC inline
int
Irq_sender::queued()
{
  return _queued;
}


/**
 * Predicate used to figure out if the sender shall be enqueued
 * for sending a second message after sending the first.
 */
PUBLIC inline NEEDS[Irq_sender::consume]
bool
Irq_sender::requeue_sender()
{ return consume() > 0; }

/**
 * Predicate used to figure out if the sender shall be dequeued after
 * sending the request.
 */
PUBLIC inline NEEDS[Irq_sender::consume]
bool
Irq_sender::dequeue_sender()
{ return consume() < 1; }

PUBLIC inline
Syscall_frame *
Irq_sender::transfer_msg(Receiver *recv)
{
  Syscall_frame* dst_regs = recv->rcv_regs();

  // set ipc return value: OK
  dst_regs->tag(L4_msg_tag(0));

  // set the IRQ label
  dst_regs->from(_irq_id);

  return dst_regs;
}

PUBLIC void
Irq_sender::modify_label(Mword const *todo, int cnt) override
{
  for (int i = 0; i < cnt*4; i += 4)
    {
      Mword const test_mask = todo[i];
      Mword const test      = todo[i+1];
      if ((_irq_id & test_mask) == test)
	{
	  Mword const set_mask = todo[i+2];
	  Mword const set      = todo[i+3];

	  _irq_id = (_irq_id & ~set_mask) | set;
	  return;
	}
    }
}

PRIVATE bool
Irq_sender::send_local(Thread *t, bool is_xcpu)
{
  // Pairs with write barrier in set_irq_thread(). Prevents to read the _irq_id
  // before _irq_thread.
  Mem::mp_rmb();

  return send_msg(t, is_xcpu);
}

PRIVATE static
Context::Drq::Result
Irq_sender::handle_remote_hit(Context::Drq *, Context *target, void *arg)
{
  Irq_sender *irq = static_cast<Irq_sender*>(arg);
  irq->set_cpu(current_cpu());

  auto t = access_once(&irq->_irq_thread);
  if (EXPECT_TRUE(t == target))
    {
      if (EXPECT_TRUE(irq->send_local(t, true)))
        return Context::Drq::no_answer_resched();
    }
  else if (EXPECT_TRUE(is_valid_thread(t)))
    t->drq(&irq->_drq, handle_remote_hit, irq, Context::Drq::No_wait);

  return Context::Drq::no_answer();
}

PRIVATE inline
Smword
Irq_sender::queue()
{
  Smword old;
  do
    old = _queued;
  while (!cas(&_queued, old, old + 1));
  return old;
}

/**
 * Enqueue Irq_sender as sender at the thread.
 *
 * \param t  Targeted thread.
 *
 * \post     As this function might directly switch to the receiving thread (IRQ
 *           shortcut), it must be assumed to be a potential preemption point.
 */
PRIVATE inline
void
Irq_sender::send(Thread *t)
{
  if (EXPECT_FALSE(t->home_cpu() != current_cpu()))
    t->drq(&_drq, handle_remote_hit, this, Context::Drq::No_wait);
  else
    send_local(t, false);
}


PUBLIC inline NEEDS[Irq_sender::send, Irq_sender::queue]
void
Irq_sender::_hit_level_irq(Upstream_irq const *ui)
{
  // We're entered holding the kernel lock, which also means irqs are
  // disabled on this CPU (XXX always correct?).  We never enable irqs
  // in this stack frame (except maybe in a nonnested invocation of
  // switch_exec() -> switchin_context()) -- they will be re-enabled
  // once we return from it (iret in entry.S:all_irqs) or we switch to
  // a different thread.

  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _queued);

  assert (cpu_lock.test());
  mask_and_ack();
  Upstream_irq::ack(ui);

  auto t = access_once(&_irq_thread);
  if (EXPECT_FALSE(!is_valid_thread(t)))
    return;

  if (queue() == 0)
    send(t);
}

PRIVATE static
void
Irq_sender::hit_level_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_level_irq(ui); }

PUBLIC inline NEEDS[Irq_sender::send, Irq_sender::queue]
void
Irq_sender::_hit_edge_irq(Upstream_irq const *ui)
{
  // We're entered holding the kernel lock, which also means irqs are
  // disabled on this CPU (XXX always correct?).  We never enable irqs
  // in this stack frame (except maybe in a nonnested invocation of
  // switch_exec() -> switchin_context()) -- they will be re-enabled
  // once we return from it (iret in entry.S:all_irqs) or we switch to
  // a different thread.

  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _queued);

  assert (cpu_lock.test());

  auto t = access_once(&_irq_thread);
  if (EXPECT_FALSE(!is_valid_thread(t)))
    {
      mask_and_ack();
      Upstream_irq::ack(ui);
      return;
    }

  Smword q = queue();

  // if we get a second edge triggered IRQ before the first is
  // handled we can mask the IRQ.  The consume function will
  // unmask the IRQ when the last IRQ is dequeued.
  if (!q)
    ack();
  else
    mask_and_ack();

  Upstream_irq::ack(ui);
  if (q == 0)
    send(t);
}

PRIVATE static
void
Irq_sender::hit_edge_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_edge_irq(ui); }


PRIVATE
L4_msg_tag
Irq_sender::sys_bind(L4_msg_tag tag, L4_fpage::Rights rights, Utcb const *utcb,
                     Utcb *utcb_out)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  Thread *thread;

  Ko::Rights t_rights;
  thread = Ko::deref<Thread>(&tag, utcb, &t_rights);
  if (!thread)
    return tag;

  if (EXPECT_FALSE(!(t_rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  return bind_irq_thread(thread, utcb, utcb_out);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_detach(L4_fpage::Rights rights, Utcb * /*utcb_out*/)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  // The following operation is a preemption point, so we have to hold a
  // reference to Irq_sender to ensure it is not deleted.
  Ref_ptr self(this);
  return commit_result(detach_irq_thread());
}


PUBLIC
L4_msg_tag
Irq_sender::kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                    Utcb const *utcb, Utcb *utcb_out)
{
  L4_msg_tag tag = f->tag();
  int op = get_irq_opcode(tag, utcb);

  if (EXPECT_FALSE(op < 0))
    return commit_result(-L4_err::EInval);

  switch (tag.proto())
    {
    case L4_msg_tag::Label_kobject:
      switch (op)
        {
        case Op_bind: // the Rcv_endpoint opcode (equal to Ipc_gate::bind_thread)
          return sys_bind(tag, rights, utcb, utcb_out);
        default:
          return commit_result(-L4_err::ENosys);
        }

    case L4_msg_tag::Label_irq:
      return dispatch_irq_proto(op, _queued < 1);

    case L4_msg_tag::Label_irq_sender:
      switch (op)
        {
        case Op_detach:
          return sys_detach(rights, utcb_out);

        default:
          return commit_result(-L4_err::ENosys);
        }
    default:
      return commit_result(-L4_err::EBadproto);
    }
}


PUBLIC inline
Mword
Irq_sender::id() const
{ return _irq_id; }



 // Irq implementation

static DEFINE_GLOBAL Global_data<Kmem_slab_t<Irq_sender>> _irq_allocator("Irq");

PRIVATE static
void *
Irq::q_alloc(Ram_quota *q)
{ return _irq_allocator->q_alloc(q); }

PRIVATE static
void
Irq::q_free(Ram_quota *q, void *f)
{ _irq_allocator->q_free(q, f); }

PUBLIC inline
void *
Irq::operator new (size_t, void *p)
{ return p; }

PUBLIC
void
Irq::operator delete (void *_l)
{
  Irq *l = static_cast<Irq *>(_l);
  assert(l->_q);
  asm ("" : "=m"(*l));
  q_free(l->_q, l);
}

PUBLIC template<typename T> inline NEEDS[Irq::q_alloc, Irq::operator new]
static
T*
Irq::allocate(Ram_quota *q)
{
  void *nq = q_alloc(q);
  if (nq)
    return new (nq) T(q);

  return 0;
}


PUBLIC explicit inline __attribute__((nonnull))
Irq::Irq(Ram_quota *q) : _q(q)
{
  assert(q);
}

PUBLIC
void
Irq::destroy(Kobject ***rl) override
{
  // Irq_base::destroy() does unbind(). Therefore call Kobject::destroy() which
  // waits until the existence lock was finally released by the last owner (the
  // existence lock was already invalidated before). Otherwise this IRQ object
  // could be immediately bound to another IRQ chip by the (current) owner of
  // the existence lock of this IRQ object.
  Kobject::destroy(rl);
  Irq_base::destroy();
}

namespace {

static Kobject_iface * FIASCO_FLATTEN
irq_sender_factory(Ram_quota *q, Space *,
                   L4_msg_tag, Utcb const *, Utcb *,
                   int *err, unsigned *)
{
  *err = L4_err::ENomem;
  return Irq::allocate<Irq_sender>(q);
}

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(irq_sender_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_irq_sender, irq_sender_factory);
}

}
