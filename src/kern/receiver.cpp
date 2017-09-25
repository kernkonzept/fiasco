INTERFACE:

#include "context.h"
#include "l4_error.h"
#include "member_offs.h"
#include "timeout.h"
#include "prio_list.h"
#include "ref_obj.h"

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
  MEMBER_OFFSET();

public:
  enum Rcv_state
  {
    Rs_not_receiving = false,
    Rs_ipc_receive   = true,
    Rs_irq_receive   = true + 1
  };

  enum Abort_state
  {
    Abt_ipc_done,
    Abt_ipc_cancel,
    Abt_ipc_in_progress,
  };

  Rcv_state sender_ok(const Sender* sender) const;

  virtual ~Receiver() = 0;

private:
  // DATA
  Sender const *_partner;         // IPC partner I'm waiting for/involved with
  Syscall_frame *_rcv_regs; // registers used for receive
  Mword _caller;
  Iteratable_prio_list _sender_list;
};

typedef Context_ptr_base<Receiver> Receiver_ptr;

IMPLEMENTATION:

#include "l4_types.h"
#include <cassert>

#include "cpu_lock.h"
#include "globals.h"
#include "lock_guard.h"
#include "logdefs.h"
#include "sender.h"
#include "thread_lock.h"
#include "entry_frame.h"
#include "std_macros.h"
#include "thread_state.h"

// Interface for receivers

IMPLEMENT inline Receiver::~Receiver() {}
/** Constructor.
    @param thread_lock the lock used for synchronizing access to this receiver
    @param space_context the space context 
 */
PROTECTED inline
Receiver::Receiver()
: Context(), _caller(0)
{}

PUBLIC inline
Receiver *
Receiver::caller() const
{ return reinterpret_cast<Receiver*>(_caller & ~0x03UL); }

PUBLIC inline
L4_fpage::Rights
Receiver::caller_rights() const
{ return L4_fpage::Rights(_caller & 0x3); }


PUBLIC inline
void
Receiver::set_caller(Receiver *caller, L4_fpage::Rights rights)
{
  Mword nv = Mword(caller) | (cxx::int_value<L4_fpage::Rights>(rights) & 0x3);
  reinterpret_cast<Mword volatile &>(_caller) = nv;
}
/** IPC partner (sender).
    @return sender of ongoing or previous IPC operation
 */
PROTECTED inline
Sender const *
Receiver::partner() const
{
  return _partner;
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
Iteratable_prio_list *
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
  _timeout = t;
  t->set(tval, home_cpu());
}

PUBLIC inline
void
Receiver::dequeue_timeout()
{
  if (_timeout)
    _timeout->dequeue(_timeout->has_hit());
}

PUBLIC inline
void
Receiver::enqueue_timeout_again()
{
  if (_timeout && Cpu::online(home_cpu()))
    _timeout->set_again(home_cpu());
}

PUBLIC inline
void
Receiver::reset_timeout()
{
  if (EXPECT_TRUE(!_timeout))
    return;

  _timeout->reset();
  _timeout = 0;
}

PROTECTED inline
void Receiver::prepare_receive(Sender *partner, Syscall_frame *regs)
{
  set_rcv_regs(regs);  // message should be poked in here
  set_partner(partner);
}

PROTECTED inline
bool Receiver::prepared() const
{ return _rcv_regs; }

/** Set the IPC partner (sender).
    @param partner IPC partner
 */
PUBLIC inline
void
Receiver::set_partner(Sender* partner)
{
  _partner = partner;
}

PUBLIC inline
bool
Receiver::in_ipc(Sender *sender)
{
  return (state() & Thread_receive_in_progress) && (partner() == sender);
}


/** Return whether the receiver is ready to accept a message from the
    given sender.
    @param sender thread that wants to send a message to this receiver
    @return true if receiver is in correct state to accept a message 
                 right now (open wait, or closed wait and waiting for sender).
 */
IMPLEMENT inline NEEDS["std_macros.h", "thread_state.h", "sender.h",
                       Receiver::partner, Receiver::vcpu_async_ipc]
Receiver::Rcv_state
Receiver::sender_ok(const Sender *sender) const
{
  unsigned ipc_state = state() & Thread_ipc_mask;

  // If Thread_send_in_progress is still set, we're still in the send phase
  if (EXPECT_FALSE(ipc_state != Thread_receive_wait))
    return vcpu_async_ipc(sender);

  // Check open wait; test if this sender is really the first in queue
  if (EXPECT_TRUE(!partner()
                  && (_sender_list.empty()
		    || sender->is_head_of(&_sender_list))))
    return Rs_ipc_receive;

  // Check closed wait; test if this sender is really who we specified
  if (EXPECT_TRUE(sender == partner()))
    return Rs_ipc_receive;

  return Rs_not_receiving;
}

//-----------------------------------------------------------------------------
// VCPU code:

PRIVATE inline
Receiver::Rcv_state
Receiver::vcpu_async_ipc(Sender const *sender) const
{
  if (EXPECT_FALSE(state() & Thread_ipc_mask))
    return Rs_not_receiving;

  Vcpu_state *vcpu = vcpu_state().access();

  if (EXPECT_FALSE(!vcpu_irqs_enabled(vcpu)))
    return Rs_not_receiving;

  Receiver *self = const_cast<Receiver*>(this);

  if (this == current())
    self->spill_user_state();

  if (self->vcpu_enter_kernel_mode(vcpu))
    vcpu = vcpu_state().access();

  LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
      l->type = 1;
      l->state = vcpu->_saved_state;
      l->ip = Mword(sender);
      l->sp = regs()->sp();
      l->space = ~0; //vcpu_user_space() ? static_cast<Task*>(vcpu_user_space())->dbg_id() : ~0;
      );

  self->_rcv_regs = &vcpu->_ipc_regs;
  vcpu->_regs.set_ipc_upcall();
  self->set_partner(const_cast<Sender*>(sender));
  self->state_add_dirty(Thread_receive_wait);
  self->vcpu_save_state_and_upcall();
  return Rs_irq_receive;
}

PUBLIC inline
void
Receiver::reset_caller()
{
  if (_caller)
    _caller = 0;
}

PUBLIC inline
void
Receiver::vcpu_update_state()
{
  if (EXPECT_TRUE(!(state() & Thread_vcpu_enabled)))
    return;

  if (sender_list()->empty())
    vcpu_state().access()->sticky_flags &= ~Vcpu_state::Sf_irq_pending;
}


// --------------------------------------------------------------------------

struct Ipc_remote_dequeue_request
{
  Receiver *partner;
  Sender *sender;
  Receiver::Abort_state state;
};

PRIVATE static
Context::Drq::Result
Receiver::handle_remote_abort_send(Drq *, Context *, void *_rq)
{
  Ipc_remote_dequeue_request *rq = (Ipc_remote_dequeue_request*)_rq;
  if (rq->sender->in_sender_list())
    {
      // really cancel IPC
      rq->state = Abt_ipc_cancel;
      rq->sender->sender_dequeue(rq->partner->sender_list());
      rq->partner->vcpu_update_state();
    }
  else if (rq->partner->in_ipc(rq->sender))
    rq->state = Abt_ipc_in_progress;
  return Drq::done();
}

PUBLIC
Receiver::Abort_state
Receiver::abort_send(Sender *sender)
{
  Ipc_remote_dequeue_request rq;
  rq.partner = this;
  rq.sender = sender;
  rq.state = Abt_ipc_done;
  if (current_cpu() != home_cpu())
    drq(handle_remote_abort_send, &rq);
  else
    handle_remote_abort_send(0, 0, &rq);
  return rq.state;
}

