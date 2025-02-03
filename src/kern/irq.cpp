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
  enum class Op : Mword
  {
    Trigger    = 2, // Irq_sender + Irq_semaphore
    Eoi        = 4, // Icu + Irq_sender + Irq_semaphore
  };

protected:
  Ram_quota *_q;
};


/**
 * IRQ kernel object to send IPC messages to a receiving thread.
 *
 * When an IRQ hits, the Irq_sender queues itself to the thread it is bound to
 * (target thread). When no thread is bound to the Irq_sender, the behaviour
 * depends on the mode. For level triggered IRQs, the IRQ is dropped because
 * its state persists in hardware. Edge triggered IRQs are marked as pending
 * and will be delivered when bound to a new thread, though.
 *
 * To enqueue the Irq_sender at the thread, the Irq_sender sends a DRQ to the
 * target thread (asynchronously). We ensure that the Irq_sender queues its
 * DRQ request to at most one thread at a time (see Irq_send_state::can_send()).
 * The _send_state queued flag is only reset by Ipc_sender via
 * Irq_sender::finish_send().
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
  public Ref_cnt_obj,
  public Vcpu_irq_list_item
{
  friend struct Irq_sender_test;

public:
  enum class Op : Mword
  {
    Attach = 0,
    Detach = 1,
    Bind_vcpu = 2,
    Bind   = 0x10,
  };

  enum Detach_result : int
  {
    Detach_inactive = 0,
    Detach_pending = 1,
    Detach_abandoned = 2,
  };

private:
  Mword _irq_id;
  // Must only be used for sending async DRQs (no answer), because for regular
  // DRQs the DRQ reply is sent to Drq::context(), which assumes that the Drq
  // object is member of a Context.
  Context::Drq _drq;
};

//-----------------------------------------------------------------------------
INTERFACE [!irq_direct_inject]:

EXTENSION class Irq_sender
{
public:
  class Irq_thread
  {
    Thread *_target;

  public:
    constexpr Irq_thread() : _target(nullptr) {}
    explicit constexpr Irq_thread(Thread *t, bool = false) : _target(t) {}

    constexpr bool operator==(Irq_thread const &other) const
    { return _target == other._target; }

    constexpr operator Thread*() const { return _target; }
    constexpr explicit operator bool() const { return is_bound(); }
    constexpr Thread* operator->() const { return _target; }

    constexpr bool is_bound() const { return _target != nullptr; }
    constexpr bool is_ipc_sender() const { return true; }
    constexpr bool is_vcpu_irq() const { return false; }

    static constexpr Irq_thread invalid() { return Irq_thread{}; };
  };

private:
  Irq_thread _irq_thread;
};

//-----------------------------------------------------------------------------
INTERFACE [irq_direct_inject]:

EXTENSION class Irq_sender
{
public:
  /**
   * Composite pointer that stores an additional boolean inline.
   */
  class Irq_thread
  {
    Mword _target;

    Thread* as_thread() const
    { return reinterpret_cast<Thread *>(_target & ~static_cast<Mword>(3)); }

  public:
    constexpr Irq_thread() : _target(0) {}
    explicit Irq_thread(Thread *t, bool vcpu = false)
    : _target(reinterpret_cast<Mword>(t) | (vcpu ? 2U : 0U))
    {}

    constexpr bool operator==(Irq_thread const &other) const
    { return _target == other._target; }

    operator Thread*() const { return as_thread(); }
    constexpr explicit operator bool() const { return is_bound(); }
    Thread* operator->() const { return as_thread(); }

    constexpr bool is_bound() const { return (_target & ~Mword{3}) != 0; }
    constexpr bool is_ipc_sender() const { return (_target & 2U) == 0; }
    constexpr bool is_vcpu_irq() const { return (_target & 2U) != 0; }

    static constexpr Irq_thread invalid() { return Irq_thread{}; };
  };

private:
  Irq_thread _irq_thread;

  struct Revoke_vcpu_irq_request
  {
    Irq_sender *self;
    Irq_thread old;
    Irq_thread target;
    Mword irq_id;
    Context::Revoke_vcpu_state result;
    bool abandon;
  };
};

//-----------------------------------------------------------------------------
INTERFACE:

EXTENSION class Irq_sender
{
public:
  /**
   * Tracks the IPC send state of an Irq_sender.
   *
   * When an IRQ hits, the Irq_sender transitions from "Not Queued" to "Queued"
   * and sends an IPC to its target thread. Once the IPC send finishes, it
   * transitions back to "Not Queued", ready to send the next IPC. If no thread
   * is bound, edge-triggered Irq_sender's will set the "Pending" state so that
   * the IRQ is queued when being bound.
   *
   * The destruction of an Irq_sender may run in parallel to an ongoing IPC send
   * the Irq_sender is involved in. Therefore it is split into two phases:
   * 1. The Invalidated state flag is set, from now on no new IPC send operation
   *    can be started.
   * 2. Once the ongoing IPC send finishes, or immediately if none is in
   *    progress, the Destroyed state flag is set (via attempt_destroy()).
   *
   * States:
   *   - Not Queued:   IPC send is not queued. A vIRQ might be pending, though.
   *   - Queued:       IPC send is queued. / vIRQ waits for available HW
   *                   resources.
   *   - Masked:       Edge-triggered IRQ was masked because a new IRQ arrived
   *                   while IPC send was still queued.
   *   - Invalidated:  Irq_sender was invalidated, no new IRQs can be queued,
   *                   but it might still be involved in an ongoing IPC send
   *                   operation.
   *   - Destroyed:    Irq_sender was invalidated and is not involved in an IPC
   *                   send operation anymore.
   *   - Pending:      IRQ received without target thread.
   *
   * Valid state transitions:
   *   - Not Queued  -> Queued | Invalidated
   *   - Queued      -> Not Queued | Masked | Invalidated
   *   - Masked      -> Not Queued | Invalidated
   *   - Invalidated -> Destroyed
   *
   * Drivers:
   *   - Irq_sender::queue()
   *   - Irq_sender::finish_send()
   *   - Irq_sender::destroy()
   *
   * \note Accessing the send state of an Irq_sender requires holding its
   *       _irq_lock.
   */
  class Irq_send_state
  {
    Mword _state;

  public:
    constexpr Irq_send_state() : Irq_send_state(0) {}
    explicit constexpr Irq_send_state(Mword state) : _state(state) {}

    enum : Mword
    {
      Queued = 1,
      Masked = 2,
      Invalidated = 4,
      Destroyed = 8,
      Pending = 16,
    };

    constexpr void set(Mword flags) { _state |= flags; }
    constexpr void clear(Mword flags) { _state &= ~flags; }

    constexpr bool is_queued() const { return _state & Queued; }
    constexpr bool is_masked() const { return _state & Masked; }
    constexpr bool is_invalidated() const { return _state & Invalidated; }
    constexpr bool is_destroyed() const { return _state & Destroyed; }
    constexpr bool is_pending() const { return _state & Pending; }

    constexpr bool can_send() const
    { return !is_queued() && !is_invalidated() && !is_destroyed(); }

    constexpr bool is_in_destruction() const
    { return is_invalidated() || is_destroyed(); }

    /**
     * Attempts to transition the Irq_sender from Invalidated to Destroyed
     * state.
     *
     * \pre Irq_send_state must be in Invalidated state.
     * \pre Must hold _irq_lock of Irq_Sender.
     *
     * \retval true if the Irq_sender transitioned into Destroyed state.
     * \retval false if the Irq_sender is still queued in an IPC send or is
     *               already in Destroyed state.
     */
    bool attempt_destroy()
    {
      assert(is_invalidated());

      if (is_queued())
        return false;

      if (is_destroyed())
        return false;

      set(Irq_send_state::Destroyed);
      return true;
    }
  };

private:
  Irq_send_state _send_state;
};


//-----------------------------------------------------------------------------
IMPLEMENTATION:

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
Mword
Irq::get_irq_opcode(L4_msg_tag tag, Utcb const *utcb)
{
  if (tag.proto() == L4_msg_tag::Label_irq && tag.words() == 0)
    return static_cast<Mword>(Op::Trigger);
  if (EXPECT_FALSE(tag.words() < 1))
    return ~0UL;

  return access_once(utcb->values) & 0xffff;
}

PROTECTED inline
L4_msg_tag
Irq::dispatch_irq_proto(Irq::Op op, bool may_unmask)
{
  switch (op)
    {
    case Op::Eoi:
      if (may_unmask)
        unmask();
      return L4_msg_tag(L4_msg_tag::Schedule); // no reply

    case Op::Trigger:
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
Irq_sender::set_irq_thread(Irq_thread target, Mword irq_id)
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
Irq_sender::Irq_thread
Irq_sender::start_replace_irq_thread(Irq_thread target, Mword irq_id)
{
  assert(_irq_lock.is_locked());

  Irq_thread old = _irq_thread;
  if (!old || old.is_ipc_sender())
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
 * \param target      The receiver that wants to receive IPC messages for this
 *                    IRQ. Might be nullptr to unbind.
 * \param irq_id      The label for the IPC send operation.
 * \param abandon     If true, abandon an active IRQ on the old vCPU. Otherwise
 *                    fail the call and keep the previous vCPU if IRQ is
 *                    currently active.
 *
 * \retval 0          On success, `target` is the new IRQ handler thread.
 *                    IPC/vCPU-IRQ was not pending.
 * \retval 1          On success, `target` is the new IRQ handler thread.
 *                    IPC/vCPU-IRQ was pending on old target thread.
 * \retval 2          vCPU-IRQ is active on old target vCPU and has been
 *                    abandoned.
 * \retval -EBusy     Failed. The vCPU-IRQ is active on old target vCPU.
 *
 * \post              As this function executes a DRQ, it must be assumed to be a
 *                    potential preemption point.
 */
PRIVATE
int
Irq_sender::finish_replace_irq_thread(Irq_thread old, Irq_thread target,
                                      Mword irq_id, bool abandon)
{
  int result;
  bool finish_send_on_unbind = true;
  bool was_pending = false;

  if (!old)
    {
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);
      if (_send_state.is_pending())
        {
          _send_state.clear(Irq_send_state::Pending);
          result = Detach_pending;
          finish_send_on_unbind = false;
        }
      else
        result = Detach_inactive;
    }
  else if ((TAG_ENABLED(irq_direct_inject)) && old.is_vcpu_irq())
    {
      // Old thread is only changed later in the DRQ handler, once we know
      // whether the revocation is successful. Therefore someone else can
      // concurrently replace it, dropping our reference to it. But that is
      // fine, because we can rely on the DRQ to be executed before the thread
      // is deleted.
      // For new target thread however we need a reference to ensure it stays
      // alive until DRQ handler is executed.
      Ref_ptr<Thread> target_ref(target);

      switch (revoke_vcpu_irq(old, target, irq_id, abandon))
        {
        case Context::Revoke_vcpu_state::Ok_was_clear:
          result = Detach_inactive;
          break;
        case Context::Revoke_vcpu_state::Ok_was_queued:
          result = Detach_pending;
          break;
        case Context::Revoke_vcpu_state::Ok_was_pending:
          result = Detach_pending;
          finish_send_on_unbind = false;
          break;
        case Context::Revoke_vcpu_state::Ok_was_active:
          // We have abandoned an active vIRQ. The VMM has to cope with the
          // eventual EOI of the guest because the vIRQ is left active...
          result = Detach_abandoned;
          break;
        case Context::Revoke_vcpu_state::Ok_was_active_and_queued:
          // Like above but the next vIRQ was already queued!
          result = Detach_abandoned;
          was_pending = true;
          break;
        case Context::Revoke_vcpu_state::Fail_is_active:
          // We do not abandon active vIRQs except when detaching. If the Irq
          // is currently active on the old vCPU, it is the responsibility of
          // the VMM to cope with this pity...
          return -L4_err::EBusy;
        }
    }
  else // old.is_ipc_sender()
    {
      switch (old->Receiver::abort_send(this))
        {
        case Receiver::Abt_ipc_done:
          result = Detach_inactive;
          break; // was not queued

        case Receiver::Abt_ipc_cancel:
          result = Detach_pending; // was queued
          break;

        default:
          // This must not happen as this is only the case for IPC including
          // message items and an IRQ never sends message items.
          panic("IRQ IPC flagged as in progress");
        }
    }

  if (old && old->dec_ref() == 0)
    delete old;

  was_pending = was_pending || result == Detach_pending;
  if (result != Detach_inactive)
    {
      // We might have been preempted during DRQ execution, i.e. the someone
      // might have re-bound to a different thread and might even have deleted
      // the old thread. We therefore have to re-read the currently bound
      // thread under the _irq_lock.
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

      // Do not forward if the Irq_sender was destroyed in the meantime.
      if (_send_state.is_in_destruction())
        target = Irq_thread::invalid();
      else
        target = _irq_thread;

      // If the edge-triggered IRQ was queued before and the Irq_sender is
      // now unbound, remember that it was pending originally.
      // Level-triggered IRQs must not be remembered because the Irq_sender
      // should only be triggered if the interrupt is still asserted when
      // being bound again.
      if (!target.is_bound() && is_edge_triggered() && was_pending)
        _send_state.set(Irq_send_state::Pending);
    }

  // re-inject if IRQ was queued before
  if (was_pending)
    {
      if (target.is_bound())
        send(target);
      else if (finish_send_on_unbind)
        finish_send();
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
 * \retval -EBusy     On error. The vCPU-IRQ is active on the old target vCPU.
 * \retval L4_error::Not_existent  Irq_sender object was deleted.
 *
 * \post This function must be assumed to be a potential preemption point (see
 *       finish_replace_irq_thread()). In particular that means the Irq_sender
 *       object might have been deleted, unless the caller holds a counted
 *       reference to it.
 */
PUBLIC
L4_msg_tag
Irq_sender::bind_irq_thread(Irq_thread target, Utcb const *utcb, Utcb *utcb_out)
{
  assert(cpu_lock.test());

  Mword irq_id = access_once(&utcb->values[1]);

  Irq_thread old;
    {
      // Grab the _irq_lock to guard against concurrent bind/unbinds.
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);
      // Prevent binding thread if the Irq is invalid.
      if (_send_state.is_in_destruction())
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
  int result = finish_replace_irq_thread(old, target, irq_id, false);
  return commit_result(result < 0 ? result : 0);
}

PUBLIC
Receiver *
Irq_sender::owner() const { return static_cast<Thread*>(_irq_thread); }

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
 * \retval 2  On success, interrupt was active on vCPU and was abandoned.
 */
PRIVATE
int
Irq_sender::detach_irq_thread()
{
  Irq_thread old;
    {
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

      if (!_irq_thread)
        return -L4_err::ENoent;

      mask();

      // Need to keep the _send_state.is_masked() state consistent with the
      // hardware for edge triggered interrupts. Otherwise we might never
      // unmask the interrupt if it's pending before being bound again.
      if (is_edge_triggered())
        _send_state.set(Irq_send_state::Masked);

      old = start_replace_irq_thread(Irq_thread::invalid(), ~0UL);
    }

  return finish_replace_irq_thread(old, Irq_thread::invalid(), ~0UL, true);
}

PUBLIC explicit
Irq_sender::Irq_sender(Ram_quota *q)
: Kobject_h<Irq_sender, Irq>(q), _irq_id(~0UL)
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
  assert(_send_state.is_destroyed());
  assert(!_drq.queued());
}

PUBLIC
void
Irq_sender::switch_mode(bool is_edge_triggered) override
{
  hit_func = is_edge_triggered ? &hit_edge_irq : &hit_level_irq;
}

PUBLIC inline
bool
Irq_sender::is_edge_triggered() const
{ return hit_func == &hit_edge_irq; }

PUBLIC
void
Irq_sender::destroy(Kobjects_list &reap_list) override
{
  assert(!_send_state.is_invalidated());

  auto g = lock_guard(cpu_lock);
  Irq::destroy(reap_list);

    {
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

      // "IPC send" reference, which keeps the Irq_sender alive while it is
      // involved in an IPC send.
      // Released after detaching or by Irq_sender::finish_send() if IPC was
      // still in progress.
      inc_ref();

      _send_state.set(Irq_send_state::Invalidated);
    }

  // At this point no new thread can be bound to the Irq_sender.
  // Note: We implicitly hold a counted reference to Irq_sender, namely the
  //       capability reference (see constructor).
  detach_irq_thread();

    {
      auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

      // Try to finish destruction of Irq_sender. If we are not successful,
      // there is still an IPC send pending, and Irq_sender::finish_send()
      // will later take care of releasing the "IPC send" reference.
      if (_send_state.attempt_destroy())
        // Cannot not drop to zero, we still hold the capability reference.
        check(dec_ref() > 0);
    }
}

/**
 * Called when an IPC send is finished (done or aborted by the receiver) to
 * update the send state of this Irq_sender.
 *
 * \pre The cpu_lock must be held.
 * \pre Must not hold _irq_lock.
 *
 * \post If Irq_sender is in Invalidated state, this function will transition it
 *       into Destroyed state and release the "IPC send" reference (see
 *       Irq_sender::destroy()). In case that was the last reference, it will
 *       also delete the Irq_sender, i.e. the caller must not use the Irq_sender
 *       object after calling this function.
 */
PUBLIC inline
void
Irq_sender::finish_send()
{
  auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

  _send_state.clear(Irq_send_state::Queued);

  if (_send_state.is_invalidated())
    {
      if (_send_state.attempt_destroy())
        {
          // Have to release the lock before deleting the IRQ sender object, as
          // lock is part of the object.
          g.reset();

          if (dec_ref() == 0)
            delete this;

          return;
        }
    }
  else if (EXPECT_FALSE(_send_state.is_masked()))
    {
      _send_state.clear(Irq_send_state::Masked);
      unmask();
    }
}

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

/**
 * Send IPC message to given target thread, whose home CPU is the current CPU or
 * an offline CPU.
 *
 * \post The caller must not use the Irq_sender object after calling this
 *       function on it, as it might have been deleted (see finish_send()).
 */
PRIVATE bool
Irq_sender::send_local(Irq_thread t, bool is_xcpu)
{
  // Pairs with write barrier in set_irq_thread(). Prevents to read the _irq_id
  // before _irq_thread.
  Mem::mp_rmb();

  if ((TAG_ENABLED(irq_direct_inject)) && t.is_vcpu_irq())
    {
      inject_vcpu_irq(t);
      return false;
    }
  else
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
  else if (EXPECT_TRUE(t.is_bound()))
    t->drq(&irq->_drq, handle_remote_hit, irq, Context::Drq::No_wait);
  else
    irq->finish_send();

  return Context::Drq::no_answer();
}

/**
 * Attempt IPC send state transition.
 *
 * \pre The _irq_lock must be held.
 *
 * \return Whether can send, i.e. state was transititioned to queued.
 */
PRIVATE inline
bool
Irq_sender::queue()
{
  if (!_send_state.can_send())
    return false;

  _send_state = Irq_send_state(Irq_send_state::Queued);
  return true;
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
Irq_sender::send(Irq_thread t)
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
  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _send_state);
  assert (cpu_lock.test());

  auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

  mask_and_ack();
  Upstream_irq::ack(ui);

  auto t = _irq_thread; // access under lock

  // For level triggered IRQs we move to the Queued state only if a thread is
  // bound. When the thread is later bound and the IRQ is still asserted, we
  // will natually end up here again.
  bool can_send = t.is_bound() && queue();
  if (can_send)
    {
      // Have to release lock before send (preemptible operation).
      g.reset();
      send(t);
    }
}

PRIVATE static
void
Irq_sender::hit_level_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_level_irq(ui); }

PUBLIC inline NEEDS[Irq_sender::send, Irq_sender::queue]
void
Irq_sender::_hit_edge_irq(Upstream_irq const *ui)
{
  // LOG_MSG_3VAL(current(), "IRQ", dbg_id(), 0, _send_state);

  assert (cpu_lock.test());

  auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);

  auto t = _irq_thread; // access under lock
  if (!t.is_bound())
    {
      // If we get an interrupt without a thread bound, we mask the IRQ. After
      // user-space binds a thread, it must unmask the IRQ. The pending state
      // is recorded so that the interrupt is not lost and delivered once the
      // Irq_sender is bound again.
      _send_state.set(Irq_send_state::Pending | Irq_send_state::Masked);
      mask_and_ack();
      Upstream_irq::ack(ui);
      return;
    }

  bool can_send = queue();
  if (can_send)
    {
      ack();
      Upstream_irq::ack(ui);
      // Have to release lock before send (preemptible operation).
      g.reset();
      send(t);
    }
  else
    {
      // If we get a second edge triggered IRQ before the first is handled we
      // can mask the IRQ. The finish_send() function will unmask the IRQ when
      // the last IRQ is dequeued.
      _send_state.set(Irq_send_state::Masked);
      mask_and_ack();
      Upstream_irq::ack(ui);
    }
}

PRIVATE static
void
Irq_sender::hit_edge_irq(Irq_base *i, Upstream_irq const *ui)
{ nonull_static_cast<Irq_sender*>(i)->_hit_edge_irq(ui); }


PRIVATE
L4_msg_tag
Irq_sender::sys_bind(L4_msg_tag tag, L4_fpage::Rights rights, Utcb const *utcb,
                     Utcb *utcb_out, bool to_vcpu)
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

  Irq_thread t = Irq_thread(thread, to_vcpu);
  return bind_irq_thread(t, utcb, utcb_out);
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
  Mword op = get_irq_opcode(tag, utcb);

  if (EXPECT_FALSE(op == ~0UL))
    return commit_result(-L4_err::EInval);

  switch (tag.proto())
    {
    case L4_msg_tag::Label_kobject:
      switch (Op{op})
        {
        case Op::Bind: // the Rcv_endpoint opcode (equal to Ipc_gate::bind_thread)
          return sys_bind(tag, rights, utcb, utcb_out, false);
        default:
          return commit_result(-L4_err::ENosys);
        }

    case L4_msg_tag::Label_irq:
      // Handling Irq::Op::Eoi here is workaround as we cannot access _irq_lock
      // in dispatch_irq_proto().
      if (Irq::Op{op} == Irq::Op::Eoi)
        {
          auto g = lock_guard<No_cpu_lock_policy>(_irq_lock);
          if (_send_state.can_send())
            unmask();
          return L4_msg_tag(L4_msg_tag::Schedule); // no reply
        }
      else
        return dispatch_irq_proto(Irq::Op{op}, false);

    case L4_msg_tag::Label_irq_sender:
      switch (Irq_sender::Op{op})
        {
        case Op::Detach:
          return sys_detach(rights, utcb_out);
        case Op::Bind_vcpu:
          return sys_bind_vcpu(tag, rights, utcb, utcb_out);

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

  return nullptr;
}


PUBLIC explicit inline __attribute__((nonnull))
Irq::Irq(Ram_quota *q) : _q(q)
{
  assert(q);
}

PUBLIC
void
Irq::destroy(Kobjects_list &reap_list) override
{
  // Irq_base::destroy() does unbind(). Therefore call Kobject::destroy() which
  // waits until the existence lock was finally released by the last owner (the
  // existence lock was already invalidated before). Otherwise this IRQ object
  // could be immediately bound to another IRQ chip by the (current) owner of
  // the existence lock of this IRQ object.
  Kobject::destroy(reap_list);
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

//--------------------------------------------------------------------------
IMPLEMENTATION[debug || test_support_code]:

PUBLIC inline
bool
Irq_sender::queued()
{
  // As this is called (only) from JDB, we probably should not take the lock
  // here, NMI sent by JDB might have interrupted a CPU (with interrupts off)
  // while holding the lock.
  // auto g = lock_guard(_irq_lock);
  return _send_state.is_queued();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [irq_direct_inject]:
PRIVATE inline
void
Irq_sender::inject_vcpu_irq(Thread *t)
{
  t->arch_inject_vcpu_irq(_irq_id, this);
}

PRIVATE static inline
Context::Drq::Result
Irq_sender::handle_revoke_vcpu_irq(Context::Drq *, Context *, void *_rq)
{
  Revoke_vcpu_irq_request *rq = static_cast<Revoke_vcpu_irq_request*>(_rq);
  Irq_sender *self = rq->self;

  // Grab the Irq lock to guard against concurrent bind/unbinds.
  auto g = lock_guard(self->_irq_lock);

  // Some other, concurrent thread could have been faster than our DRQ.
  if (self->_irq_thread != rq->old)
    {
      rq->result = Context::Revoke_vcpu_state::Ok_was_clear;
      return Context::Drq::done();
    }

  rq->result = self->_irq_thread->arch_revoke_vcpu_irq(self, rq->abandon);

  // We do not abandon active vIRQs except when detaching. If the Irq is
  // currently active on the old vCPU, it is the responsibility of the VMM to
  // cope with this pity...
  if (rq->result == Context::Revoke_vcpu_state::Fail_is_active)
    return Context::Drq::done();

  // Update the target only after the vCPU vIRQ was revoked because that might
  // fail. This is safe because the code runs on the CPU where the old
  // interrupt is supposed to hit so it cannot be triggered concurrently.
  self->set_irq_thread(rq->target, rq->irq_id);

  return Context::Drq::done();
}

PRIVATE inline
Context::Revoke_vcpu_state
Irq_sender::revoke_vcpu_irq(Irq_thread old, Irq_thread target, Mword irq_id,
                            bool abandon)
{
  Revoke_vcpu_irq_request rq;
  rq.self = this;
  rq.old = old;
  rq.target = target;
  rq.irq_id = irq_id;
  rq.abandon = abandon;

  // Execute the replacement in the context of the current receiver thread to
  // prevent races with actual irq triggers. This is also a precondition of
  // Context::arch_revoke_vcpu_irq().
  if (old && current_cpu() != old->home_cpu())
    old->drq(handle_revoke_vcpu_irq, &rq);
  else
    handle_revoke_vcpu_irq(nullptr, nullptr, &rq);

  return rq.result;
}

PUBLIC
void
Irq_sender::vcpu_soi() override
{
  finish_send();
}

PUBLIC
void
Irq_sender::vcpu_eoi() override
{
  if (!is_edge_triggered())
    unmask();
}

PUBLIC
Mword
Irq_sender::vcpu_irq_id() const override
{ return _irq_id; }

PRIVATE inline
L4_msg_tag
Irq_sender::sys_bind_vcpu(L4_msg_tag tag, L4_fpage::Rights rights,
                          Utcb const *utcb, Utcb *utcb_out)
{
  return sys_bind(tag, rights, utcb, utcb_out, true);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [!irq_direct_inject]:

PRIVATE inline
void
Irq_sender::inject_vcpu_irq(Thread *)
{}

PRIVATE inline
Context::Revoke_vcpu_state
Irq_sender::revoke_vcpu_irq(Thread *, Irq_thread, Mword, bool)
{ return Context::Revoke_vcpu_state::Ok_was_clear; }

PRIVATE inline
L4_msg_tag
Irq_sender::sys_bind_vcpu(L4_msg_tag, L4_fpage::Rights, Utcb const *,
                          Utcb *)
{
  return commit_result(-L4_err::ENosys);
}
