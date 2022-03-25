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

public:
  enum Op
  {
    Op_eoi_1      = 0, // Irq_sender + Irq_semaphore
    Op_compat_attach     = 1,
    Op_trigger    = 2, // Irq_sender + Irq_semaphore
    Op_compat_chain      = 3,
    Op_eoi_2      = 4, // Icu + Irq_sender + Irq_semaphore
    Op_compat_detach     = 5,
  };

protected:
  Ram_quota *_q;
  Context::Drq _drq;
};


/**
 * IRQ Kobject to send IPC messages to a receiving thread.
 */
class Irq_sender
: public Kobject_h<Irq_sender, Irq>,
  public Ipc_sender<Irq_sender>,
  public Vcpu_irq_list_item
{
public:
  enum Op {
    Op_attach = 0,
    Op_detach = 1,
    Op_bind_vcpu = 2,
    Op_mask_vcpu = 3,
    Op_unmask_vcpu = 4,
    Op_clear_vcpu = 5,
    Op_set_priority = 6,
    Op_bind     = 0x10,
  };

protected:
  static Mword detach_in_progress()
  { return 3; }

  static bool is_valid_thread(Mword t)
  { return t > detach_in_progress(); }

  Smword _queued;
  Mword _irq_thread;

private:
  static inline Thread *as_thread(Mword t)
  { return reinterpret_cast<Thread *>(t & ~(Mword)1); }

  static inline Mword as_irq_sender(Thread *t)
  { return (Mword)t; }

  static inline Mword as_vcpu_irq(Thread *t)
  { return (Mword)t | 1U; }

  static inline bool is_irq_sender(Mword t)
  { return (t & 1U) == 0; }

  static inline bool is_vcpu_irq(Mword t)
  { return (t & 1U) != 0; }

  Mword _irq_id;
  bool _vcpu_enabled;
  Unsigned8 _irq_prio;
};


//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include "assert_opt.h"
#include "atomic.h"
#include "config.h"
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
    case Op_eoi_1:
    case Op_eoi_2:
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

/**
 * Bind a receiver to this device interrupt.
 * \param t           the receiver that wants to receive IPC messages for this
 *                    IRQ
 * \param rl[in,out]  the list of objects that have to be destroyed. The
 *                    operation might append objects to this list if it is in
 *                    charge of deleting the old receiver that used to be
 *                    attached to this IRQ.
 *
 * \retval 0        on success, `t` is the new IRQ handler thread
 * \retval -EINVAL  if `t` is not a valid thread.
 * \retval -EBUSY   if another detach operation is in progress.
 */
PUBLIC inline NEEDS ["atomic.h", "cpu_lock.h", "lock_guard.h"]
int
Irq_sender::alloc(Mword t, Kobject ***rl)
{
  if (!is_valid_thread(t))
    return -L4_err::EInval;

  Mword prev;
  for (;;)
    {
      prev = access_once(&_irq_thread);

      if (prev == t)
        return 0;

      if (EXPECT_FALSE(prev == detach_in_progress()))
        return -L4_err::EBusy;

      if (mp_cas(&_irq_thread, prev, t))
        break;
    }

  Mem::mp_acquire();

  auto g = lock_guard(cpu_lock);
  bool reinject = false;

  if (is_valid_thread(prev))
    {
      Thread *old = as_thread(prev);
      if (is_irq_sender(prev))
        switch (old->Receiver::abort_send(this))
          {
          case Receiver::Abt_ipc_done:
            break; // was not queued

          case Receiver::Abt_ipc_cancel:
            reinject = true;
            break;

          default:
            // this must not happen as this is only the case
            // for IPC including message items and an IRQ never
            // sends message items.
            panic("IRQ IPC flagged as in progress");
          }
      else
        reinject = old->arch_revoke_vcpu_irq(this, is_irq_sender(t));

      old->put_n_reap(rl);
    }

  Thread *next = as_thread(t);
  next->inc_ref();
  if (Cpu::online(next->home_cpu()))
    _chip->set_cpu(pin(), next->home_cpu());

  return reinject ? 1 : 0;
}

PUBLIC
Receiver *
Irq_sender::owner() const { return as_thread(_irq_thread); }

/**
 * Release an interrupt.
 * \param rl[in,out]  The list of objects that have to be destroyed.
 *                    The operation might append objects to this list if it is
 *                    in charge of deleting the receiver that used to be
 *                    attached to this IRQ.
 *
 * \retval 0        on success.
 * \retval -ENOENT  if there was no receiver attached.
 * \retval -EBUSY   when there is another detach operation in progress.
 */
PRIVATE
int
Irq_sender::free(Kobject ***rl)
{
  Mem::mp_release();
  Mword t;
  for (;;)
    {
      t = access_once(&_irq_thread);

      if (t == detach_in_progress())
        return -L4_err::EBusy;

      if (t == 0)
        return -L4_err::ENoent;

      if (EXPECT_TRUE(mp_cas(&_irq_thread, t, detach_in_progress())))
        break;
    }

  auto guard = lock_guard(cpu_lock);
  mask();

  Thread *old = as_thread(t);
  if (is_irq_sender(t))
    old->Receiver::abort_send(this);
  else
    old->arch_revoke_vcpu_irq(this, true);

  Mem::mp_release();
  write_now(&_irq_thread, 0);
  // release cpu-lock early, actually before delete
  guard.reset();

  old->put_n_reap(rl);
  return 0;
}

PUBLIC explicit
Irq_sender::Irq_sender(Ram_quota *q = 0)
: Kobject_h<Irq_sender, Irq>(q), _queued(0), _irq_thread(0), _irq_id(~0UL),
  _vcpu_enabled(false), _irq_prio(255)
{
  hit_func = &hit_level_irq;
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
  (void)free(rl);
  Irq::destroy(rl);
}


/** Consume one interrupt.
    @return number of IRQs that are still pending.
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
  while (!mp_cas (&_queued, old, old - 1));
  Mem::mp_acquire();

  if (old == 2 && hit_func == &hit_edge_irq)
    unmask();

  return old - 1;
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
Irq_sender::send(Mword dest, bool is_not_xcpu)
{
  Thread *t = as_thread(dest);
  if (is_vcpu_irq(dest))
    {
      if (_vcpu_enabled)
        t->arch_inject_vcpu_irq(_irq_id, this);
      // FIXME: we should increase the irq priority if we injected into a
      // running guest. Otherwise lower priority irqs of the guest may still
      // trap into Fiasco!
      return false;
    }
  else
    return send_msg(t, _irq_prio, is_not_xcpu);
}

PRIVATE static
Context::Drq::Result
Irq_sender::handle_remote_hit(Context::Drq *, Context *target, void *arg)
{
  Irq_sender *irq = (Irq_sender*)arg;
  irq->set_cpu(current_cpu());
  auto dest = access_once(&irq->_irq_thread);
  Thread *t = as_thread(dest);
  if (EXPECT_TRUE(t == target))
    {
      if (EXPECT_TRUE(irq->send(dest, false)))
        return Context::Drq::no_answer_resched();
    }
  else if (EXPECT_TRUE(is_valid_thread(dest)))
    t->drq(&irq->_drq, handle_remote_hit, irq,
           Context::Drq::No_wait);

  return Context::Drq::no_answer();
}

PRIVATE inline
Smword
Irq_sender::queue()
{
  Smword old;
  do
    old = _queued;
  while (!mp_cas(&_queued, old, old + 1));
  return old;
}


PRIVATE inline
void
Irq_sender::send(Mword dest)
{
  Thread *t = as_thread(dest);
  if (EXPECT_FALSE(t->home_cpu() != current_cpu()))
    t->drq(&_drq, handle_remote_hit, this,
           Context::Drq::No_wait);
  else
    send(dest, true);
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
                     Syscall_frame *, bool to_vcpu)
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

  Reap_list rl;
  int res = alloc(to_vcpu ? as_vcpu_irq(thread) : as_irq_sender(thread),
                  rl.list());

  // note: this is a possible race on user-land
  // where the label of an IRQ might become inconsistent with the attached
  // thread. The user is responsible to synchronize Irq::attach calls to prevent
  // this.
  if (res >= 0)
    {
      _irq_id = access_once(&utcb->values[1]);
      if (_irq_prio > thread->sched_context()->prio())
        _irq_prio = thread->sched_context()->prio();
      _chip->set_priority(pin(), _irq_prio);
    }

  // re-inject if it was queued before
  if (res > 0)
    {
      // might have changed between the CAS and taking the lock
      Mword t = access_once(&_irq_thread);
      if (EXPECT_TRUE(is_valid_thread(t)))
        send(t);
    }

  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();

  return commit_result(res >= 0 ? 0 : res);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_detach(L4_fpage::Rights rights)
{
  if (EXPECT_FALSE(!(rights & L4_fpage::Rights::CS())))
    return commit_result(-L4_err::EPerm);

  Reap_list rl;
  auto res = free(rl.list());
  _irq_id = ~0UL;
  cpu_lock.clear();
  rl.del();
  cpu_lock.lock();
  return commit_result(res);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_set_priority(Utcb const *utcb)
{
  int prio = access_once(&utcb->values[1]);
  if (prio < 0 || prio > 255)
    return commit_result(-L4_err::EInval);

  Mword t = access_once(&_irq_thread);
  if (is_valid_thread(t) && prio > as_thread(t)->sched_context()->prio())
    prio = as_thread(t)->sched_context()->prio();

  _irq_prio = prio;
  return commit_result(_chip->set_priority(pin(), _irq_prio));
}

PUBLIC
L4_msg_tag
Irq_sender::kinvoke(L4_obj_ref, L4_fpage::Rights rights, Syscall_frame *f,
                    Utcb const *utcb, Utcb *)
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
          return sys_bind(tag, rights, utcb, f, false);
        default:
          return commit_result(-L4_err::ENosys);
        }

    case L4_msg_tag::Label_irq:
      return dispatch_irq_proto(op, _queued < 1);

    case L4_msg_tag::Label_irq_sender:
      switch (op)
        {
        case Op_detach:
          return sys_detach(rights);
        case Op_bind_vcpu:
          return sys_bind_vcpu(tag, rights, utcb, f);
        case Op_mask_vcpu:
          return sys_mask_vcpu();
        case Op_unmask_vcpu:
          return sys_unmask_vcpu();
        case Op_clear_vcpu:
          return sys_clear_vcpu();
        case Op_set_priority:
          return sys_set_priority(utcb);

        default:
          return commit_result(-L4_err::ENosys);
        }
    default:
      return commit_result(-L4_err::EBadproto);
    }
}

PUBLIC
Mword
Irq_sender::obj_id() const override
{ return _irq_id; }



 // Irq implementation

static Kmem_slab_t<Irq_sender> _irq_allocator("Irq");

PRIVATE static
void *
Irq::q_alloc(Ram_quota *q)
{ return _irq_allocator.q_alloc(q); }

PRIVATE static
void
Irq::q_free(Ram_quota *q, void *f)
{
  if (q)
    _irq_allocator.q_free(q, f);
  else
    _irq_allocator.free(f);
}

PUBLIC inline
void *
Irq::operator new (size_t, void *p)
{ return p; }

PUBLIC
void
Irq::operator delete (void *_l)
{
  Irq *l = reinterpret_cast<Irq*>(_l);
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


PUBLIC explicit inline
Irq::Irq(Ram_quota *q = 0) : _q(q) {}

PUBLIC
void
Irq::destroy(Kobject ***rl) override
{
  Irq_base::destroy();
  Kobject::destroy(rl);
}

namespace {
static Kobject_iface * FIASCO_FLATTEN
irq_sender_factory(Ram_quota *q, Space *,
                   L4_msg_tag, Utcb const *,
                   int *err)
{
  *err = L4_err::ENomem;
  return Irq::allocate<Irq_sender>(q);
}

static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_irq_sender, irq_sender_factory);
}
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [irq_direct_inject]:

PUBLIC
bool
Irq_sender::vcpu_eoi() override
{
  bool pending = consume() > 0;
  if (_queued < 1)
    unmask();
  return pending;
}

PUBLIC
Mword
Irq_sender::vcpu_irq_id() const override
{ return _irq_id; }

PRIVATE
L4_msg_tag
Irq_sender::sys_bind_vcpu(L4_msg_tag tag, L4_fpage::Rights rights,
                          Utcb const *utcb, Syscall_frame *f)
{
  return sys_bind(tag, rights, utcb, f, true);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_mask_vcpu()
{
  _vcpu_enabled = false;

  Mword t = access_once(&_irq_thread);
  if (is_valid_thread(t) && is_vcpu_irq(t))
    as_thread(t)->arch_revoke_vcpu_irq(this, false);

  return commit_result(0);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_unmask_vcpu()
{
  _vcpu_enabled = true;

  bool pending = _queued > 0;
  Mword t = access_once(&_irq_thread);
  if (is_valid_thread(t) && is_vcpu_irq(t) && pending)
    send(t);

  return commit_result(0);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_clear_vcpu()
{
  _queued = 0;
  Mword t = access_once(&_irq_thread);
  if (is_valid_thread(t) && is_vcpu_irq(t))
    as_thread(t)->arch_revoke_vcpu_irq(this, false);

  return commit_result(0);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [!irq_direct_inject]:

PRIVATE
L4_msg_tag
Irq_sender::sys_bind_vcpu(L4_msg_tag, L4_fpage::Rights, Utcb const *,
                          Syscall_frame *)
{
  return commit_result(-L4_err::ENosys);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_mask_vcpu()
{
  return commit_result(-L4_err::ENosys);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_unmask_vcpu()
{
  return commit_result(-L4_err::ENosys);
}

PRIVATE
L4_msg_tag
Irq_sender::sys_clear_vcpu()
{
  return commit_result(-L4_err::ENosys);
}
