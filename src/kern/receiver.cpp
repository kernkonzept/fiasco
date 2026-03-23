INTERFACE:

#include "atomic.h"
#include "context.h"
#include "l4_error.h"
#include "member_offs.h"
#include "timeout.h"
#include "prio_list.h"
#include "ref_obj.h"
#include "reply_space.h"

class Syscall_frame;
class Sender;


/** A receiver.  This is a role class, and real receiver's must inherit from 
    it.  The protected interface is intended for the receiver, and the public
    interface is intended for the sender.

    The only reason this class inherits from Context is to force a specific 
    layout for Thread.  Otherwise, Receiver could just embed or reference
    a Context.
 */
class Receiver : public Context,  public Ref_cnt_obj
{
  friend class Jdb_tcb;
  friend class Jdb_thread;

  MEMBER_OFFSET();

public:
  struct Rcv_state
  {
    enum S
    {
      Not_receiving = 0x00,
      Open_wait_flag = 0x01,
      Ipc_receive   = 0x02, // with closed wait
      Ipc_open_wait = 0x03, // IPC with open wait
      Irq_receive   = 0x05, // IRQ (always open wait)
    };

    S s;

    constexpr Rcv_state(S s) noexcept : s(s) {}
    Rcv_state() = default;
    Rcv_state(Rcv_state const &) = default;
    Rcv_state(Rcv_state &&) = default;
    Rcv_state &operator = (Rcv_state const &) = default;
    Rcv_state &operator = (Rcv_state &&) = default;
    ~Rcv_state() = default;

    constexpr explicit operator bool () const { return s; }
    constexpr bool is_open_wait() const { return s & Open_wait_flag; }
    constexpr bool is_irq() const { return s & 0x4; }
    constexpr bool is_ipc() const { return s & 0x2; }
  };

  enum Abort_state
  {
    /// IPC was already finished -- IPC not aborted. No error.
    Abt_ipc_done,
    /// IPC was really aborted, error code will be set.
    Abt_ipc_cancel,
    /// Abort while IPC already in progress. IPC operation will be finished.
    /// No error.
    Abt_ipc_in_progress,
  };

  Rcv_state sender_ok(const Sender* sender) const;

  /// Whether the IPC partner this Receiver is waiting for (closed wait) is
  /// enqueued in the Receiver's sender list
  bool partner_in_sender_list() const { return _partner_in_sender_list; }

  virtual ~Receiver() = 0;

  /// Constant used to address the implicit per-thread reply cap slot.
  static constexpr auto Implicit_reply_cap_index = Reply_cap_index(~0U);

protected:
  /**
   * Unconditionally reset a reply cap slot. If valid, also the
   * `_partner_reply_cap` of the pointed to caller is reset.
   *
   * \pre Interrupts must be disabled.
   *
   * \param slot  Slot that shall be reset
   * \return Value that was stored in reply cap slot prior to reset.
   */
  static inline Reply_cap reset_reply_cap_slot(Reply_cap_slot *slot)
  {
    Reply_cap old_reply_cap = slot->reset_atomic();
    Receiver *caller = old_reply_cap.caller();

    if (caller)
      // The data dependency pairs with mp_wmb in Receiver::set_reply_cap(). No
      // need for a Mem::mp_rmb().
      ::cas<Reply_cap_slot *>(&caller->_partner_reply_cap, slot, nullptr);

    return old_reply_cap;
  }

private:
  /**
   * IPC partner this Receiver is waiting for/involved with.
   *
   * Not reset after a receive operation is finished, so it might contain an old
   * value from the last receive operation (see also `Receiver::in_ipc()`).
   *
   * Must never be dereferenced, only compared.
   */
  void const *_partner;

  /**
   * The caller remembers the reply cap slot that the callee used for the call
   * for which the caller is currently waiting for a reply.
   *
   * Unlike `_partner` this must only be set while caller is waiting for callee
   * to respond, otherwise nullptr. This is essential, because when aborting
   * the wait for a call response, the caller uses `_partner_reply_cap` to
   * detect if callee is still alive (was not deleted in the meantime). This is
   * reset together with the reply cap slot its points to, either by the callee
   * or the caller, whoever comes first.
   *
   * Furthermore, a Receiver (caller) may only be referenced by at most one
   * reply cap slot, namely the one its `_partner_reply_cap` points to.
   */
  Reply_cap_slot *_partner_reply_cap = nullptr;

  /// Registers used for receive.
  Syscall_frame *_rcv_regs;

  /// Implicit, per-receiver reply cap slot. Used if no explicit slot is
  /// selected.
  Reply_cap_slot _implicit_reply_cap;

  Iterable_prio_list _sender_list;
  // Tracks whether the IPC partner this Receiver is waiting for, as specified
  // in `_partner`, is enqueued in the Receiver's sender list.
  bool _partner_in_sender_list;
};

typedef Context_ptr_base<Receiver> Receiver_ptr;

// ---------------------------------------------------------------------------
INTERFACE[explicit_reply_caps]:

EXTENSION class Receiver
{
private:
  inline Reply_cap_slot *get_cur_reply_cap_slot()
  { return _reply_cap; }

  inline void set_cur_reply_cap_slot(Reply_cap_slot *cap)
  { _reply_cap = cap; }

  /// Chosen reply cap slot. Can either point to _implicit_reply_cap or an
  /// explicit reply capability slot in the Reply_space of the receiver.
  Reply_cap_slot *_reply_cap;
};

// ---------------------------------------------------------------------------
INTERFACE[!explicit_reply_caps]:

EXTENSION class Receiver
{
private:
  inline Reply_cap_slot *get_cur_reply_cap_slot()
  { return &_implicit_reply_cap; }

  inline void set_cur_reply_cap_slot(Reply_cap_slot *)
  {}
};

// ---------------------------------------------------------------------------
IMPLEMENTATION:

#include "atomic.h"
#include "l4_types.h"
#include <cassert>

#include "cpu.h"
#include "cpu_lock.h"
#include "globals.h"
#include "lock_guard.h"
#include "logdefs.h"
#include "sender.h"
#include "entry_frame.h"
#include "std_macros.h"
#include "thread_state.h"

// Interface for receivers

IMPLEMENT inline Receiver::~Receiver()
{
  // Ensure _partner_reply_cap was reset before destruction.
  assert(_partner_reply_cap == nullptr);
}

PROTECTED inline
Receiver::Receiver()
: Context()
{}

/**
 * Set up the reply cap slot in the callee's reply cap space and remember that
 * reply cap slot in the caller's `_partner_reply_cap`.
 *
 * \pre Must only be invoked on the callee, either in the context of the caller
 *      or of the callee, when both participants are blocking on one another,
 *      transferring the IPC message, i.e. Thread_ipc_transfer flag is set on
 *      the other participant.
 * \pre The `caller` must not already be referenced by a reply cap slot.
 * \pre Interrupts must be disabled.
 *
 * \param caller  Caller waiting for callee's response.
 * \param rights  Rights callee has when responding to the caller.
 *
 * \note The prerequisite ensures that this function is never executed
 *       concurrently with `reset_partner_reply_cap()` for a caller.
 *       However, a reply cap slot can be reset at any time by an unrelated
 *       thread, so concurrent execution with `reset_reply_cap()` is possible.
 */
PUBLIC inline
void
Receiver::set_reply_cap(Receiver *caller, L4_fpage::Rights rights)
{
  Reply_cap_slot *cur_slot = get_cur_reply_cap_slot();

  // If reply cap slot is already used, reset it first. For the case that
  // multiple threads in the same space operate concurrently on the same slot,
  // see the cas below.
  if (cur_slot->is_used()) [[unlikely]]
    {
      reset_reply_cap_slot(cur_slot);
      WARN("%p: Application bug: reply capability overwritten!\n", this);
    }

  // It is crucial that at this point the caller is not referenced by any reply
  // cap slot (see prerequisite). Otherwise concurrent execution of
  // reset_reply_cap() on that reply cap slot could result in an inconsistent
  // state, where we in the end successfully set up the reply cap slot with the
  // cas below, but reset_reply_cap() overwrote our previous write to
  // `caller->_partner_reply_cap` with nullptr, leaving us with a dangling
  // pointer to caller in the reply cap slot.
  assert(caller->_partner_reply_cap == nullptr);

  // First record in the caller the reply cap slot used for the call.
  atomic_store(&caller->_partner_reply_cap, cur_slot);

  // Because someone unrelated can, at any time, reset the reply cap slot, we
  // must ensure that this someone sees an up-to-date value of
  // _partner_reply_cap, before they see our write to _reply_cap, so they
  // can properly reset _partner_reply_cap. This is crucial to avoid a dangling
  // pointer in _partner_reply_cap in case of concurrent reply cap slot usage.
  Mem::mp_wmb(); // Pairs with data dependency in reset_reply_cap_slot().

  Reply_cap new_value = Reply_cap(caller, rights);
  if constexpr (TAG_ENABLED(explicit_reply_caps))
    {
      // If the cas fails, someone else was faster, we give up, that is to be
      // considered a user-space error. They messed up their reply cap slot
      // allocation.
      if (!cur_slot->cas(Reply_cap::Empty(), new_value)) [[unlikely]]
        {
          // Both caller and callee are blocked, so the only thing that can
          // happen is that the value in _reply_cap was changed concurrently by
          // another receiver thread in the same task. In that case we cannot
          // use the reply cap slot, so rollback our write to
          // _partner_reply_cap from above.
          atomic_store(&caller->_partner_reply_cap, nullptr);

          WARN("%p: Concurrent usage of reply cap slot detected. "
               "Broken reply cap allocator?\n", this);
        }
    }
  else
    // Only the current thread can access the implicit reply cap slot. It is
    // guaranteed to be available due to the reset_reply_cap_slot() above.
    cur_slot->write_atomic(new_value);
}

/**
 * Reset the `_partner_reply_cap` of a caller. If valid, also the reply cap
 * slot it referred to is reset.
 *
 * There are two use cases for this function:
 *   - Caller aborted waiting for a response to a call (timed out or canceled).
 *   - Callee sends to caller directly, without using the reply cap.
 *
 * \pre Must only be executed by the caller, or by a potential sender in the
 *      context of the caller, if that sender has checked that caller is not in
 *      an IPC operation with someone else (via `caller->is_partner(sender)`).
 *      That ensures we do not interfere with an ongoing IPC operation of the
 *      caller.
 * \pre Interrupts must be disabled.
 *
 * \note The prerequisite ensures that this function is never executed
 *       concurrently with `set_reply_cap()` for a caller.
 *       However, a reply cap slot can be reset at any time by an unrelated
 *       thread, so concurrent execution with `reset_reply_cap()` is possible.
 */
PUBLIC inline NEEDS["atomic.h"]
void
Receiver::reset_partner_reply_cap()
{
  assert(cpu_lock.test());

  // Seeing an outdated value of _partner_reply_cap here does not matter. That
  // could only ever happen, if someone else was faster than us in resetting
  // _partner_reply_cap, in which case they are required to also take care of
  // the pointed to reply cap slot. But since we use cas below there is no
  // danger of creating an inconsistent state.

  // And the RCU mechanism ensures that if we do not yet see the write, the
  // object containing the pointed to reply cap slot must still be alive.
  Reply_cap_slot *reply_cap_slot = atomic_load(&_partner_reply_cap);
  if (reply_cap_slot != nullptr) [[unlikely]]
    {
      // Someone might concurrently reset this to nullptr, but that does
      // not bother us, nullptr is nullptr.
      atomic_store(&_partner_reply_cap, nullptr);

      // This is safe, we only exchange if the reply cap refers to us (caller).
      // There is no way we can end up in the reply cap again in the meantime,
      // i.e. make another call by chance with the new callee using the same
      // reply cap slot, as it would require our cooperation, which is
      // impossible as this code is executing in our context.
      Reply_cap old_value =
        Reply_cap(this, reply_cap_slot->read_dirty().caller_rights());
      // If the cas fails, the reply cap slot was reused/reset by someone else
      // in the meantime, but that does not bother us.
      reply_cap_slot->cas(old_value, Reply_cap::Empty());
    }
}

/**
 * Get reply cap at given reply cap slot.
 *
 * \note Only use for debugging purposes.
 */
PUBLIC inline
Reply_cap
Receiver::get_reply_cap(Reply_cap_index reply_cap_index) const
{
  Reply_cap_slot const *reply_cap;
  if (reply_cap_index == Implicit_reply_cap_index)
    reply_cap = &_implicit_reply_cap;
  else
    reply_cap = space()->get_reply_cap(reply_cap_index);

  return reply_cap ? reply_cap->read_atomic() : Reply_cap::Empty();
}

/**
 * Unconditionally reset the reply cap slot. If valid, also the
 * `_partner_reply_cap` of the pointed to caller is reset.
 *
 * \pre Must only be invoked by a callee on itself or in a context where the
 *      callee is known to be prevented from executing concurrently.
 * \pre Interrupts must be disabled.
 *
 * \return Value that was stored in reply cap slot prior to reset.
 */
PUBLIC inline
Reply_cap
Receiver::reset_reply_cap(Reply_cap_index reply_cap_index)
{
  assert(cpu_lock.test());

  Reply_cap_slot *reply_cap_slot_ptr;
  if (reply_cap_index == Implicit_reply_cap_index)
    {
      reply_cap_slot_ptr = &_implicit_reply_cap;
    }
  else
    {
      reply_cap_slot_ptr = space()->access_reply_cap(reply_cap_index, false);

      // In case invalid index was provided.
      if (reply_cap_slot_ptr == nullptr) [[unlikely]]
        return Reply_cap::Empty();
    }

  return reset_reply_cap_slot(reply_cap_slot_ptr);
}

/**
 * Check if the given sender is stored as the IPC partner of this Receiver.
 *
 * The IPC partner field is not reset after a receive operation is finished, so
 * it might contain an old value from the last receive operation (see also
 * `Receiver::in_ipc()`).
 *
 * \pre Must only be invoked in a context where it is safe to access the
 *      Receiver's state, e.g. on the Receiver's home CPU or in a DRQ targeted
 *      at the Receiver.
 */
PROTECTED inline
bool Receiver::is_partner(Sender *sender) const
{
  return _partner == sender;
}

// Interface for senders

/** Return a reference to receiver's IPC registers.
    Senders call this function to poke values into the receiver's register set.
    @pre state() & Thread_ipc_receiving_mask
    @return pointer to receiver's IPC registers.
 */
PUBLIC inline NEEDS[<cassert>]
Syscall_frame*
Receiver::rcv_regs() const
{
  //assert (state () & Thread_ipc_receiving_mask);

  return _rcv_regs;
}

/** Head of sender list.
    @return a reference to the receiver's list of senders
 */
PUBLIC inline
Iterable_prio_list *
Receiver::sender_list()
{
  return &_sender_list;
}

// MANIPULATORS

PROTECTED inline
void
Receiver::set_rcv_regs(Syscall_frame* regs)
{
  _rcv_regs = regs;
}

PUBLIC inline
void
Receiver::set_timeout(Timeout *t)
{
  _timeout = t;
}

PUBLIC inline
void
Receiver::set_timeout(Timeout *t, Unsigned64 tval)
{
  assert(current_cpu() == home_cpu());

  _timeout = t;
  t->set(tval);
}

PUBLIC inline NEEDS["cpu.h"]
void
Receiver::enqueue_timeout_again()
{
  if (_timeout && Cpu::online(home_cpu()))
    {
      assert(current_cpu() == home_cpu());
      _timeout->set_again();
    }
}

PUBLIC inline
void
Receiver::reset_timeout()
{
  if (!_timeout) [[likely]]
    return;

  _timeout->reset();
  _timeout = nullptr;
}

PROTECTED inline
bool Receiver::prepare_receive(Sender *partner, Syscall_frame *regs)
{
  bool open_wait = !partner && regs;
  if (open_wait)
    {
      Reply_cap_slot *slot = &_implicit_reply_cap;
      if (regs->ref().explicit_reply())
        {
          if constexpr (TAG_ENABLED(explicit_reply_caps))
            {
              auto reply_cap_index = regs->ref().reply_cap();
              slot = space()->access_reply_cap(reply_cap_index, true);
              if (slot == nullptr) [[unlikely]]
                return false;
            }
          else
            return false;
        }

      set_cur_reply_cap_slot(slot);
    }

  set_rcv_regs(regs);  // message should be poked in here
  set_partner(partner);

  // At this point we know that a potential communication partner for our
  // receive phase (closed wait) is still alive. Therefore, the following check
  // is safe.
  _partner_in_sender_list = partner != nullptr && partner->in_sender_list()
                            && partner->wait_queue() == sender_list();
  return true;
}

/**
 * Update receiver state after sender is enqueued to sender list.
 */
PUBLIC inline
void
Receiver::on_sender_enqueued(Sender *sender)
{
  vcpu_set_irq_pending();
  if (is_partner(sender))
    _partner_in_sender_list = true;
}

/**
 * Update receiver state after sender is dequeued from sender list.
 */
PUBLIC inline
void
Receiver::on_sender_dequeued(Sender *sender)
{
  vcpu_update_state();
  if (is_partner(sender))
    _partner_in_sender_list = false;
}

PROTECTED inline
bool Receiver::prepared() const
{ return _rcv_regs; }

/**
 * Set the IPC partner (sender).
 *
 * \pre Must only be invoked in a context where it is safe to access the
 *      Receiver's state, e.g. on the Receiver's home CPU or in a DRQ targeted
 *      at the Receiver.
 *
 * \param partner IPC partner
 */
PUBLIC inline
void
Receiver::set_partner(Sender* partner)
{
  _partner = partner;
}

/**
 * Check if this Receiver is in an IPC receive operation with the given sender.
 *
 * \pre Must only be invoked in a context where it is safe to access the
 *      Receiver's state, e.g. on the Receiver's home CPU or in a DRQ targeted
 *      at the Receiver.
 */
PUBLIC inline
bool
Receiver::in_ipc(Sender *sender)
{
  return (state() & Thread_receive_in_progress) && (_partner == sender);
}


/** Return whether the receiver is ready to accept a message from the
    given sender.
    @param sender thread that wants to send a message to this receiver
    @return true if receiver is in correct state to accept a message 
                 right now (open wait, or closed wait and waiting for sender).
 */
IMPLEMENT inline NEEDS["std_macros.h", "thread_state.h", "sender.h",
                       Receiver::vcpu_async_ipc]
Receiver::Rcv_state
Receiver::sender_ok(const Sender *sender) const
{
  unsigned ipc_state = state() & Thread_ipc_mask;

  // If Thread_send_in_progress is still set, we're still in the send phase
  if (ipc_state != Thread_receive_wait) [[unlikely]]
    return vcpu_async_ipc(sender);

  // Check open wait; test if this sender is really the first in queue
  if (!_partner
      && (_sender_list.empty() || sender->is_head_of(&_sender_list))) [[likely]]
    return Rcv_state::Ipc_open_wait;

  // Check closed wait; test if this sender is really who we specified
  if (sender == _partner) [[likely]]
    return Rcv_state::Ipc_receive;

  return Rcv_state::Not_receiving;
}

//-----------------------------------------------------------------------------
// VCPU code:

PRIVATE inline NEEDS["logdefs.h"]
Receiver::Rcv_state
Receiver::vcpu_async_ipc(Sender const *sender) const
{
  if (state() & Thread_ipc_mask) [[unlikely]]
    return Rcv_state::Not_receiving;

  Vcpu_state *vcpu = vcpu_state().access();

  if (!vcpu_irqs_enabled(vcpu)) [[unlikely]]
    return Rcv_state::Not_receiving;

  Receiver *self = const_cast<Receiver*>(this);

  if (this == current())
    self->spill_user_state();

  if (self->vcpu_enter_kernel_mode(vcpu))
    vcpu = vcpu_state().access();

  LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
      l->type = 1;
      l->state = vcpu->_saved_state;
      l->ip = reinterpret_cast<Mword>(sender);
      l->sp = regs()->sp();
      l->space = ~0; //vcpu_user_space() ? static_cast<Task*>(vcpu_user_space())->dbg_id() : ~0;
      );

  self->_rcv_regs = &vcpu->_ipc_regs;
  self->set_cur_reply_cap_slot(&self->_implicit_reply_cap);
  vcpu->_regs.set_ipc_upcall();
  self->set_partner(const_cast<Sender*>(sender));
  self->state_add_dirty(Thread_receive_wait);
  self->vcpu_save_state_and_upcall_async_ipc();
  return Rcv_state::Irq_receive;
}

PUBLIC inline
void
Receiver::vcpu_update_state()
{
  if (!(state() & Thread_vcpu_enabled)) [[likely]]
    return;

  if (sender_list()->empty())
    vcpu_state().access()->sticky_flags &= ~Vcpu_state::Sf_irq_pending;
}


// --------------------------------------------------------------------------

struct Ipc_remote_dequeue_request
{
  Receiver *partner;
  Sender *sender;
  Context *sender_context;
  Receiver::Abort_state state;
};

PRIVATE static
Context::Drq::Result
Receiver::handle_remote_abort_send(Drq *, Context *, void *_rq)
{
  Ipc_remote_dequeue_request *rq = static_cast<Ipc_remote_dequeue_request*>(_rq);
  if (rq->sender->in_sender_list())
    {
      // really cancel IPC
      rq->state = Abt_ipc_cancel;
      rq->sender->sender_dequeue(rq->partner->sender_list());
      rq->partner->on_sender_dequeued(rq->sender);
    }
  else if (rq->partner->in_ipc(rq->sender))
    {
      // Only if the sender is a thread, not for Irq_sender.
      if (rq->sender_context)
        /* The Thread_ipc_transfer flag must be set here, because otherwise
         * the eventual clear by Thread::ipc_send_msg(), once the transfer is
         * finished, can occur before the sender sets Thread_ipc_transfer in
         * Thread::abort_send().
         */
        rq->sender_context->xcpu_state_change(~0UL, Thread_ipc_transfer);

      rq->state = Abt_ipc_in_progress;
    }
  return Drq::done();
}

PUBLIC
Receiver::Abort_state
Receiver::abort_send(Sender *sender, Context *sender_context)
{
  Ipc_remote_dequeue_request rq;
  rq.partner = this;
  rq.sender = sender;
  rq.sender_context = sender_context;
  rq.state = Abt_ipc_done;
  if (current_cpu() != home_cpu())
    drq(handle_remote_abort_send, &rq);
  else
    handle_remote_abort_send(nullptr, nullptr, &rq);
  return rq.state;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PROTECTED inline
bool Receiver::has_partner() const
{
  return _partner != nullptr;
}
