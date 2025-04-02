INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Thread
{
protected:
  struct Log_pf_invalid : public Tb_entry
  {
    Mword pfa;
    Cap_index cap_idx;
    Mword err;
    void print(String_buffer *buf) const;
  };
  static_assert(sizeof(Log_pf_invalid) <= Tb_entry::Tb_entry_size);

  struct Log_exc_invalid : public Tb_entry
  {
    Cap_index cap_idx;
    void print(String_buffer *buf) const;
  };
  static_assert(sizeof(Log_exc_invalid) <= Tb_entry::Tb_entry_size);
};

INTERFACE:

#include "l4_buf_iter.h"
#include "l4_error.h"

class Syscall_frame;

EXTENSION class Thread
{
protected:
  struct Check_sender
  {
    enum R
    {
      Open_wait_flag = 0x1, // not used as function return value
      Ok_closed_wait = 0,
      Ok_open_wait = 0 | Open_wait_flag,
      Queued = 2,
      Done   = 4,
      Failed = 5,
    };

    static_assert(unsigned{Rcv_state::Open_wait_flag} == unsigned{Open_wait_flag},
                  "Rcv_state and Check_sender flags must be compatible");

    R s;

    constexpr Check_sender(Receiver::Rcv_state s)
    : s(static_cast<R>(unsigned{s.s} & unsigned{Open_wait_flag}))
    {}

    constexpr Check_sender(R s) noexcept : s(s) {}
    Check_sender() = default;

    constexpr bool is_ok() const { return s == Ok_closed_wait || s == Ok_open_wait; }
    constexpr bool is_open_wait() const { return s & Open_wait_flag; }
  };

  struct Ipc_remote_request
  {
    L4_msg_tag tag;
    Thread *partner;
    bool zero_timeout;
    bool have_rcv;

    Thread::Check_sender result;
  };

  // Used by Thread::ipc_send_msg().
  Syscall_frame *_snd_regs;
  Mword _from_spec;
  // Used when the IPC receiver executes ipc_send_msg() in the context of the
  // next sender. Otherwise we can use `rights` from `do_ipc()` directly.
  L4_fpage::Rights _ipc_send_rights;
};

class Buf_utcb_saver
{
public:
  Buf_utcb_saver(Utcb const *u);
  void restore(Utcb *u);
private:
  L4_buf_desc buf_desc;
  Mword buf[2];
};

/**
 * Save critical contents of UTCB during nested IPC.
 */
class Pf_msg_utcb_saver : public Buf_utcb_saver
{
public:
  Pf_msg_utcb_saver(Utcb const *u);
  void restore(Utcb *u);
private:
  Mword msg[2];
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

// IPC setup, and handling of ``short IPC'' and page-fault IPC

// IDEAS for enhancing this implementation: 

// Volkmar has suggested a possible optimization for
// short-flexpage-to-long-message-buffer transfers: Currently, we have
// to resort to long IPC in that case because the message buffer might
// contain a receive-flexpage option.  An easy optimization would be
// to cache the receive-flexpage option in the TCB for that case.
// This would save us the long-IPC setup because we wouldn't have to
// touch the receiver's user memory in that case.  Volkmar argues that
// cases like that are quite common -- for example, imagine a pager
// which at the same time is also a server for ``normal'' requests.

// The handling of cancel and timeout conditions could be improved as
// follows: Cancel and Timeout should not reset the ipc_in_progress
// flag.  Instead, they should just set and/or reset a flag of their
// own that is checked every time an (IPC) system call wants to go to
// sleep.  That would mean that IPCs that do not block are not
// cancelled or aborted.
//-

#include <cstdlib>		// panic()

#include "l4_types.h"
#include "l4_msg_item.h"

#include "config.h"
#include "cpu_lock.h"
#include "ipc_timeout.h"
#include "lock_guard.h"
#include "logdefs.h"
#include "map_util.h"
#include "processor.h"
#include "timer.h"
#include "warn.h"

PUBLIC
void
Thread::ipc_receiver_aborted() override
{
  assert (cpu_lock.test());
  assert (wait_queue());
  set_wait_queue(nullptr);

  utcb().access()->error = L4_error::Canceled;

  if (xcpu_state_change(~0UL, Thread_transfer_failed | Thread_ready, true))
    current()->switch_to_locked(this);
}

/**
 * Receiver has to spill its FPU state before cross-core IPC that transfers FPU
 * state.
 *
 * \pre Must be called in the context of the receiver.
 */
PRIVATE inline
void
Thread::prepare_xcpu_ipc_transfer_fpu()
{
  if (_utcb_handler || utcb().access()->inherit_fpu())
    spill_fpu_if_owner();
}

/**
 * Receiver-ready callback. Receivers call this function in the context of a
 * waiting sender when they get ready to receive a message from that sender
 * (in this case a thread).
 */
PRIVATE
void
Thread::ipc_send_msg(Receiver *recv, bool open_wait) override
{
  Syscall_frame *regs = _snd_regs;

  if (EXPECT_FALSE(home_cpu() != recv->home_cpu() && regs->tag().transfer_fpu()))
    nonull_static_cast<Thread*>(recv)->prepare_xcpu_ipc_transfer_fpu();

  sender_dequeue(recv->sender_list());
  recv->vcpu_update_state();
  bool success = transfer_msg(regs->tag(), nonull_static_cast<Thread*>(recv),
                              _ipc_send_rights, open_wait);

  Mword state_del;
  Mword state_add;
  if (EXPECT_TRUE(success))
    {
      regs->tag(L4_msg_tag(regs->tag(), 0));
      state_del = Thread_ipc_mask | Thread_ipc_transfer;
      state_add = Thread_ready;
      if (Receiver::prepared())
        state_add |= Thread_receive_wait;
    }
  else
    {
      regs->tag(L4_msg_tag(regs->tag(), L4_msg_tag::Error));
      state_del = Thread_ipc_transfer; // handle Abt_ipc_in_progress in
                                       // Thread::abort_send()
      state_add = Thread_transfer_failed | Thread_ready;
    }
  if (xcpu_state_change(~state_del, state_add, true))
    recv->switch_to_locked(this);
}

PUBLIC
void
Thread::modify_label(Mword const *todo, int cnt) override
{
  Mword l = _from_spec;
  for (int i = 0; i < cnt*4; i += 4)
    {
      Mword const test_mask = todo[i];
      Mword const test      = todo[i+1];
      if ((l & test_mask) == test)
        {
          Mword const del_mask = todo[i+2];
          Mword const add_mask = todo[i+3];

          l = (l & ~del_mask) | add_mask;
          _from_spec = l;
          return;
        }
    }
}


/** Page fault handler.
    This handler suspends any ongoing IPC, then sets up page-fault IPC.
    Finally, the ongoing IPC's state (if any) is restored.
    @param pfa page-fault virtual address
    @param error_code page-fault error code.
 */
PRIVATE
bool
Thread::handle_page_fault_pager(Thread_ptr const &_pager,
                                Address pfa, Mword error_code,
                                L4_msg_tag::Protocol protocol)
{
  if (EXPECT_FALSE((state() & Thread_alien)))
    return false;

  auto guard = lock_guard(cpu_lock);

  L4_fpage::Rights rights;
  Kobject_iface *pager = _pager.ptr(space(), &rights);

  if (!pager)
    {
      WARN("CPU%u: Pager of %lx is invalid (pfa=" L4_PTR_FMT
           ", errorcode=" L4_PTR_FMT ") to %lx (pc=%lx)\n",
           cxx::int_value<Cpu_number>(current_cpu()), dbg_id(), pfa,
           error_code, cxx::int_value<Cap_index>(_pager.raw()), regs()->ip());


      LOG_TRACE("Page fault invalid pager", "ipfh", this, Log_pf_invalid,
                l->cap_idx = _pager.raw();
                l->err     = error_code;
                l->pfa     = pfa);

      kill();
      return true;
    }

  // set up a register block used as an IPC parameter block for the
  // page fault IPC
  Syscall_frame r;

  // save the UTCB fields affected by PF IPC
  Mword vcpu_irqs = vcpu_disable_irqs();
  Mem::barrier();
  Utcb *utcb = this->utcb().access(true);

  increment_mbt_counter();

  Pf_msg_utcb_saver saved_utcb_fields(utcb);


  utcb->buf_desc = L4_buf_desc(0, 0, 0, L4_buf_desc::Inherit_fpu);
  utcb->buffers[0] = L4_buf_item::map().raw();
  utcb->buffers[1] = L4_fpage::all_spaces().raw();

  utcb->values[0] = PF::addr_to_msgword0(pfa, error_code);
  utcb->values[1] = regs()->ip(); //PF::pc_to_msgword1 (regs()->ip(), error_code));

  L4_timeout_pair timeout(L4_timeout::Never, L4_timeout::Never);

  L4_msg_tag tag(2, 0, 0, protocol);

  r.timeout(timeout);
  r.tag(tag);
  r.from(0);
  r.ref(L4_obj_ref(_pager.raw(), L4_obj_ref::Ipc_call_ipc));
  pager->invoke(r.ref(), rights, &r, utcb);


  bool success = true;

  if (EXPECT_FALSE(r.tag().has_error()))
    {
      if (utcb->error.snd_phase()
          && (utcb->error.error() == L4_error::Not_existent)
          && PF::is_usermode_error(error_code)
          && !(state() & Thread_cancel))
        {
          success = false;
        }
    }
  else // no error
    {
      // If the pager rejects the mapping, it replies with a negative label
      if (EXPECT_FALSE (r.tag().proto() < 0))
        success = false;
    }

  // restore previous IPC state

  saved_utcb_fields.restore(utcb);
  Mem::barrier();
  vcpu_restore_irqs(vcpu_irqs);
  return success;
}

/**
 * In the context of the receiver, check if the sender may send to us.
 * \param sender        The sender to check.
 * \param zero_timeout  True if a zero timeout was specified, false otherwise.
 * \retval Check_sender::Ok_closed_wait Receiver ready to receive, closed wait.
 * \retval Check_sender::Ok_open_wait Receiver ready to receive, open wait.
 * \retval Check_sender::Failed (1) Receiver does not exist, or (2) Receiver
 *                              not ready and zero timeout specified.
 * \retval Check_sender::Queued Receiver not ready and timeout specified.
 */
PRIVATE inline
Thread::Check_sender
Thread::check_sender(Thread *sender, bool zero_timeout)
{
  if (EXPECT_FALSE(is_invalid()))
    {
      sender->utcb().access()->error = L4_error::Not_existent;
      return Check_sender::Failed;
    }

  if (auto ok = sender_ok(sender))
    return ok; // note Check_sender(Rcv_state) magic!

  if (zero_timeout)
    {
      sender->utcb().access()->error = L4_error::Timeout;
      return Check_sender::Failed;
    }

  sender->set_wait_queue(sender_list());
  sender->sender_enqueue(sender_list(), sender->sched_context()->prio());
  vcpu_set_irq_pending();
  return Check_sender::Queued;
}

/**
 * Setup a IPC-like timer for the given timeout.
 * \param timeout  The L4 ABI timeout value that shall be used
 * \param utcb     The UTCB that might contain an absolute timeout
 * \param timer    The timeout/timer object that shall be queued.
 *
 * This function does nothing if the timeout is *never*.
 * Sets Thread_ready and Thread_timeout in the thread state
 * if the timeout is zero or has already hit (is in the past).
 * Or enqueues the given timer object with the finite timeout calculated
 * from `timeout`.
 *
 * \retval true   The timeout has been set up or is never.
 * \retval false  The timeout has already hit (is in the past) or is zero.
 */
PUBLIC inline NEEDS["timer.h"]
bool
Thread::setup_timer(L4_timeout timeout, Utcb const *utcb, Timeout *timer)
{
  if (EXPECT_TRUE(timeout.is_never()))
    return true;

  if (EXPECT_FALSE(timeout.is_zero()))
    {
      state_add_dirty(Thread_ready | Thread_timeout);
      return false;
    }

  assert (!_timeout);

  Unsigned64 clock = Timer::system_clock();
  Unsigned64 tval = timeout.microsecs(clock, utcb);

  if (EXPECT_TRUE((tval > clock)))
    {
      set_timeout(timer, tval);
      return true;
    }
  else // timeout already hit
    {
      state_add_dirty(Thread_ready | Thread_timeout);
      return false;
    }
}

/**
 * Wait until unlocked by an IPC sender or by a pre-programmed IPC timeout.
 *
 * The thread blocks until it is unblocked again, i.e. its ready flag is set, by
 * an IPC sender or by a pre-programmed IPC timeout.
 *
 * \param sender  Communication partner to receive from (closed wait), use
 *                nullptr for an open wait.
 *
 * \pre IPC Timeout, if any, must already be set up.
 */
PRIVATE inline
void
Thread::do_receive_wait(Sender *sender)
{
  state_del_dirty(Thread_ready);

  if (sender == this)
    switch_sched(sched(), &Sched_context::rq.current());

  schedule();

  assert (state() & Thread_ready);
}



/**
 * @pre cpu_lock must be held
 */
PRIVATE inline NEEDS["logdefs.h"]
Thread::Check_sender
Thread::handshake_receiver(Thread *partner, L4_timeout snd_t)
{
  assert(cpu_lock.test());

  Check_sender r = partner->check_sender(this, snd_t.is_zero());
  switch (r.s)
    {
    case Check_sender::Failed:
      break;
    case Check_sender::Queued:
      state_add_dirty(Thread_send_wait);
      break;
    default: // Ok_closed_wait / Ok_open_wait
      partner->state_change_dirty(~(Thread_ipc_mask | Thread_ready), Thread_ipc_transfer);
      break;
    }
  return r;
}

PRIVATE inline
void
Thread::set_ipc_error(L4_error const &e, Thread *rcv)
{
  utcb().access()->error = e;
  rcv->utcb().access()->error = L4_error(e, L4_error::Rcv);
}


PRIVATE inline
Sender *
Thread::get_next_sender(Sender *sender)
{
  if (!sender_list()->empty())
    {
      if (sender) // closed wait
        {
          if (EXPECT_TRUE(sender->in_sender_list())
              && EXPECT_TRUE(sender_list() == sender->wait_queue()))
            return sender;
          return nullptr;
        }
      else // open wait
        {
          Sender *next = Sender::cast(sender_list()->first());
          assert (next->in_sender_list());
          set_partner(next);
          return next;
        }
    }
  return nullptr;
}

PRIVATE inline
bool
Thread::activate_ipc_partner(Thread *partner, Cpu_number current_cpu,
                             bool do_switch)
{
  if (partner->home_cpu() == current_cpu)
    {
      partner->state_change_dirty(~Thread_ipc_transfer, Thread_ready);
      if (do_switch)
        {
          schedule_if(switch_exec_locked(partner, Not_Helping) != Switch::Ok);
          return true;
        }
      else
        return deblock_and_schedule(partner);
    }

  partner->xcpu_state_change(~Thread_ipc_transfer, Thread_ready);
  return false;
}

/**
 * Send an IPC message and/or receive an IPC message.
 *
 * \param tag           message tag; specifies details about the send phase
 * \param from_spec     label for the receiver
 * \param partner       communication partner in the send phase; nullptr if
 *                      there is no send phase
 * \param have_receive  enable/disable receive phase
 * \param sender        communication partner in the receive phase; use
 *                      `nullptr` to accept any communication partner (open
 *                      wait)
 * \param t             timeouts for send phase and receive phase
 * \param regs          IPC registers of the initiator of this IPC
 * \param rights        object permissions; usually the permissions of the
 *                      invoked capability during a syscall
 *
 * \pre `cpu_lock` must be held
 * \pre may only be called on current_thread()
 *
 * This function blocks until the message can be sent/received, the respective
 * timeout hits, the IPC is canceled, or the thread is killed.
 *
 * \todo review closed wait handling of sender during possible
 *       quiescent states and blocking.
 */
PUBLIC
void
Thread::do_ipc(L4_msg_tag const &tag, Mword from_spec, Thread *partner,
               bool have_receive, Sender *sender, L4_timeout_pair t,
               Syscall_frame *regs, L4_fpage::Rights rights)
{
  assert (cpu_lock.test());
  assert (this == current());

  bool do_switch = false;

  assert (!(state() & Thread_ipc_mask));

  prepare_receive(sender, have_receive ? regs : nullptr);
  bool activate_partner = false;
  Cpu_number current_cpu = ::current_cpu();

  if (partner)
    {
      reset_caller(partner);

      assert(!in_sender_list());
      do_switch = tag.do_switch();

      bool ok;
      Check_sender result;

      _ipc_send_rights = rights;
      _from_spec = from_spec;

      if (EXPECT_TRUE(current_cpu == partner->home_cpu()))
        result = handshake_receiver(partner, t.snd);
      else
        {
          // We have either per se X-CPU IPC or we ran into an IPC during
          // migration (indicated by the pending DRQ).
          // This flag also prevents the receive path from accessing the thread
          // state of a remote sender.
          do_switch = false;
          _snd_regs = regs;
          result = remote_handshake_receiver(tag, partner, have_receive, t.snd);

          // this may block, so we could have been migrated here
          current_cpu = ::current_cpu();
        }

      switch (result.s)
        {
        case Check_sender::Done:
          ok = true;
          break;

        case Check_sender::Queued:
          // set _snd_regs to enable active receiving
          _snd_regs = regs;
          ok = do_send_wait(partner, t.snd); // --- blocking point ---
          current_cpu = ::current_cpu();
          break;

        case Check_sender::Failed:
          state_del_dirty(Thread_ipc_mask);
          ok = false;
          break;

        default:
          // ping pong with timeouts will profit from resetting the receiver´s
          // timeout, because it will require much less sorting overhead. If we
          // don't reset the timeout, the probability is very high that the
          // receiver timeout is in the timeout queue.
          if (EXPECT_TRUE(current_cpu == partner->home_cpu()))
            partner->reset_timeout();

          ok = transfer_msg(tag, partner, rights, result.is_open_wait());

          // transfer is also a possible migration point
          current_cpu = ::current_cpu();

          // switch to receiving state
          state_del_dirty(Thread_ipc_mask);
          if (ok && have_receive)
            state_add_dirty(Thread_receive_wait);

          activate_partner = partner != this;
          break;
        }

      if (EXPECT_FALSE(!ok))
        {
          // Send failed. Skip the receive phase (Thread_receive_wait was not
          // set) but still activate the partner (may include a switch to it)
          // to inform the partner about the failed IPC.
          regs->tag(L4_msg_tag(0, 0, L4_msg_tag::Error, 0));
        }
    }
  else
    {
      assert (have_receive);
      state_add_dirty(Thread_receive_wait);
    }

  {
    IPC_timeout rcv_timeout;
    // Indicates whether the receive timeout had already expired (is in the past
    // or is zero) when attempting to set up the timer for it.
    bool rcv_timeout_expired = false;

    // Holds the next sender if the IPC has a receive phase.
    Sender *next = nullptr;

    // A: If the send phase failed, it did not set the Thread_receive_wait flag
    // and the receive phase is skipped.
    // B: If we completed the send phase of a cross-core IPC directly on the
    // remote CPU (Check_sender::Done), we set the Thread_receive_wait flag with
    // xcpu_state_change. Between the execution of that state change and our
    // return from remote_handshake_receiver, a potential sender can now start
    // an IPC transfer to us, i.e. it makes us enter the receive phase
    // (Thread_ipc_transfer) or even directly completes the receive phase,
    // whereby in both cases also the Thread_receive_wait flag is cleared again.
    have_receive = state() & Thread_receive_wait;

    if (have_receive)
      {
        assert (!in_sender_list());
        assert (!(state() & Thread_send_wait));
        next = get_next_sender(sender);

        if (!next)
          // If there is no next sender yet, we have to set up the receive
          // timeout, either for a direct switch to the IPC partner or for a
          // do_receive_wait.
          rcv_timeout_expired = !setup_timer(t.rcv, utcb().access(true), &rcv_timeout);
      }

    if (activate_partner)
      {
        // Directly switch to the IPC partner (receiver) only if all of the
        // following apply:
        //  - The Schedule flag is not set in the message tag, i.e. the sender
        //    is willing to donate its remaining time-slice to the receiver
        //    (provided by `do_switch`).
        //  - The home CPU of the receiver is the current CPU (provided by
        //    `do_switch`).
        //  - If the IPC has a receive phase, there must be no next sender
        //    already pending (next != nullptr implicates a receive phase) and
        //    the receive timeout must not have expired yet.
        //  - The IPC transfer qualifies for time-slice donation, i.e. the
        //    sender transitions into a closed wait (call) or the sender runs
        //    on a foreign scheduling context.
        bool do_direct_switch = do_switch && !next && !rcv_timeout_expired
          && (   (have_receive && sender) // closed wait (call)
              || (Sched_context::rq.current().current_sched() != sched()));

        if (activate_ipc_partner(partner, current_cpu, do_direct_switch))
          {
            // blocked so might have a new sender queued
            have_receive = state() & Thread_receive_wait;
            if (have_receive && !next)
              next = get_next_sender(sender);
          }
      }

    if (next)
      {
        // Receive timeout might already been hit here if the next sender was
        // queued after we activated the IPC partner. In that case ignore the
        // timeout (clear the timeout flag) and transfer the message from the
        // pending sender anyway.
        state_change_dirty(~(Thread_ipc_mask | Thread_timeout), Thread_receive_in_progress);
        next->ipc_send_msg(this, !sender);
        state_del_dirty(Thread_ipc_mask);
      }
    else if (have_receive)
      {
        // If the receive timeout has not yet been hit and the IPC has not been
        // cancelled, enter receive wait.
        if ((state() & Thread_full_ipc_mask) == Thread_receive_wait)
          do_receive_wait(sender);
      }
  }

  if (sender && sender == partner)
    partner->reset_caller(this);

  Mword state = this->state();

  if (EXPECT_TRUE (!(state & Thread_full_ipc_mask)))
    return;

  while (EXPECT_FALSE(state & Thread_ipc_transfer))
    {
      state_del_dirty(Thread_ready);
      schedule();
      state = this->state();
   }

  if (EXPECT_TRUE (!(state & Thread_full_ipc_mask)))
    return;

  if (state & Thread_ipc_mask)
    {
      Utcb *utcb = this->utcb().access(true);
      // the IPC has not been finished.  could be timeout or cancel
      // XXX should only modify the error-code part of the status code

      if (EXPECT_FALSE(state & Thread_cancel))
        {
          // we've presumably been reset!
          regs->tag(commit_error(utcb, L4_error::R_canceled, regs->tag()));
        }
      else
        regs->tag(commit_error(utcb, L4_error::R_timeout, regs->tag()));
    }
  state_del(Thread_full_ipc_mask);
}


PRIVATE inline NEEDS [Thread::copy_utcb_to]
bool
Thread::transfer_msg(L4_msg_tag tag, Thread *receiver,
                     L4_fpage::Rights rights, bool open_wait)
{
  Syscall_frame* dst_regs = receiver->rcv_regs();

  bool success = copy_utcb_to(tag, receiver, rights);
  tag.set_error(!success);
  dst_regs->tag(tag);
  dst_regs->from(_from_spec);

  // setup the reply capability in case of a call
  if (success && open_wait && is_partner(receiver))
    receiver->set_caller(this, rights);

  return success;
}



IMPLEMENT inline
Buf_utcb_saver::Buf_utcb_saver(const Utcb *u)
{
  buf_desc = u->buf_desc;
  buf[0] = u->buffers[0];
  buf[1] = u->buffers[1];
}

IMPLEMENT inline
void
Buf_utcb_saver::restore(Utcb *u)
{
  u->buf_desc = buf_desc;
  u->buffers[0] = buf[0];
  u->buffers[1] = buf[1];
}

IMPLEMENT inline
Pf_msg_utcb_saver::Pf_msg_utcb_saver(Utcb const *u) : Buf_utcb_saver(u)
{
  msg[0] = u->values[0];
  msg[1] = u->values[1];
}

IMPLEMENT inline
void
Pf_msg_utcb_saver::restore(Utcb *u)
{
  Buf_utcb_saver::restore(u);
  u->values[0] = msg[0];
  u->values[1] = msg[1];
}


/**
 * \pre must run with local IRQs disabled (CPU lock held)
 * to ensure that handler does not disappear meanwhile.
 */
PRIVATE
bool
Thread::exception(Kobject_iface *handler, Trap_state *ts, L4_fpage::Rights rights)
{
  Syscall_frame r;
  L4_timeout_pair timeout(L4_timeout::Never, L4_timeout::Never);

  CNT_EXC_IPC;

  Mword vcpu_irqs = vcpu_disable_irqs();
  Mem::barrier();

  void *old_utcb_handler = _utcb_handler;
  _utcb_handler = ts;

  // fill registers for IPC
  Utcb *utcb = this->utcb().access(true);
  Buf_utcb_saver saved_state(utcb);

  utcb->buf_desc = L4_buf_desc(0, 0, 0, L4_buf_desc::Inherit_fpu);
  utcb->buffers[0] = L4_buf_item::map().raw();
  utcb->buffers[1] = L4_fpage::all_spaces().raw();

  // clear regs
  L4_msg_tag tag(L4_exception_ipc::Msg_size, 0, L4_msg_tag::Transfer_fpu,
                 L4_msg_tag::Label_exception);

  r.tag(tag);
  r.timeout(timeout);
  r.from(0);
  r.ref(L4_obj_ref(_exc_handler.raw(), L4_obj_ref::Ipc_call_ipc));
  spill_user_state();
  handler->invoke(r.ref(), rights, &r, utcb);
  fill_user_state();

  saved_state.restore(utcb);

  state_del(Thread_in_exception);
  if (!r.tag().has_error()
      && r.tag().proto() == L4_msg_tag::Label_allow_syscall)
    state_add(Thread_dis_alien);

  // restore original utcb_handler
  _utcb_handler = old_utcb_handler;
  Mem::barrier();
  vcpu_restore_irqs(vcpu_irqs);

  // FIXME: handle not existing exception handler properly
  // for now, just ignore any errors
  return 1;
}

/* return 1 if exception could be handled
 * return 0 if not for send_exception and halt thread
 */
PUBLIC inline NEEDS["task.h", "trap_state.h",
                    Thread::vcpu_return_to_kernel]
int
Thread::send_exception(Trap_state *ts)
{
  assert(cpu_lock.test());

  Vcpu_state *vcpu = vcpu_state().access();

  if (vcpu_exceptions_enabled(vcpu))
    {
      if (_exc_cont.valid(ts))
        return 1;

      // before entering kernel mode to have original fpu state before
      // enabling fpu
      save_fpu_state_to_utcb(ts, utcb().access());

      spill_user_state();

      if (vcpu_enter_kernel_mode(vcpu))
        {
          // enter_kernel_mode has switched the address space from user to
          // kernel space, so reevaluate the address of the VCPU state area
          vcpu = vcpu_state().access();
        }

      LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
          l->type = 2;
          l->state = vcpu->_saved_state;
          l->ip = ts->ip();
          l->sp = ts->sp();
          l->trap = ts->trapno();
          l->err = ts->error();
          l->space = vcpu_user_space() ? static_cast<Task*>(vcpu_user_space())->dbg_id() : ~0;
          );
      vcpu->_regs.s = *ts;
      vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_sp, vcpu_state().usr().get());
    }

  L4_fpage::Rights rights = L4_fpage::Rights(0);
  Kobject_iface *handler = _exc_handler.ptr(space(), &rights);

  if (EXPECT_FALSE(!handler))
    {
      /* no exception handler (anymore), put thread to sleep */
      LOG_TRACE("Exception invalid handler", "ieh", this, Log_exc_invalid,
                l->cap_idx = _exc_handler.raw());
      if (EXPECT_FALSE(space()->is_sigma0()))
        {
          ts->dump();
          panic("Sigma0 raised an exception");
        }

      handler = this; // block on ourselves
    }

  state_change(~Thread_cancel, Thread_in_exception);

  return exception(handler, ts, rights);
}

PRIVATE static
bool
Thread::try_transfer_local_id(L4_buf_iter::Item const *const buf,
                              L4_fpage sfp, L4_return_item_writer return_item,
                              Thread *snd, Thread *rcv)
{
  if (buf->b.is_rcv_id())
    {
      if (snd->space() == rcv->space())
        {
          return_item.set_rcv_type_flexpage(sfp);
          return true;
        }
      else
        {
          Obj_space::Capability cap = snd->space()->lookup(sfp.obj_index());
          Kobject_iface *o = cap.obj();
          if (EXPECT_TRUE(o && o->is_local(rcv->space())))
            {
              Mword rights = cap.rights()
                             & cxx::int_value<L4_fpage::Rights>(sfp.rights());
              return_item.set_rcv_type_id(o->obj_id(), rights);
              return true;
            }
        }
    }
  return false;
}

PRIVATE [[nodiscard]] static inline
bool
Thread::copy_utcb_to_utcb(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                          L4_fpage::Rights rights)
{
  assert (cpu_lock.test());

  Utcb *snd_utcb = snd->utcb().access();
  Utcb *rcv_utcb = rcv->utcb().access();
  Mword s = tag.words();
  Mword r = Utcb::Max_words;

  Mem::memcpy_mwords(rcv_utcb->values, snd_utcb->values, r < s ? r : s);

  bool success = true;
  if (tag.items())
    success = transfer_msg_items(tag, snd, snd_utcb, rcv, rcv_utcb, rights);

  if (success
      && tag.transfer_fpu()
      && rcv_utcb->inherit_fpu()
      && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  return success;
}


PUBLIC [[nodiscard]] inline NEEDS[Thread::copy_utcb_to_ts,
                                  Thread::copy_utcb_to_utcb,
                                  Thread::copy_ts_to_utcb]
bool
Thread::copy_utcb_to(L4_msg_tag tag, Thread* receiver,
                     L4_fpage::Rights rights)
{
  // we cannot copy trap state to trap state!
  assert (!this->_utcb_handler || !receiver->_utcb_handler);
  if (EXPECT_FALSE(this->_utcb_handler != nullptr))
    return copy_ts_to_utcb(tag, this, receiver, rights);
  else if (EXPECT_FALSE(receiver->_utcb_handler != nullptr))
    return copy_utcb_to_ts(tag, this, receiver, rights);
  else
    return copy_utcb_to_utcb(tag, this, receiver, rights);
}

PRIVATE static inline
Task *
Thread::transfer_msg_lookup_dst_tsk(Thread *rcv,
                                    L4_buf_iter::Item const *const buf)
{
  Task *rcv_tsk = nonull_static_cast<Task*>(rcv->space());

  // Regular receive buffer? -> destination task is implicitly the task of the
  // receiving thread.
  if (EXPECT_TRUE(!buf->b.forward_mappings()))
    return rcv_tsk;

  // Compond receive buffer -> receive task is selected explicitly.
  L4_obj_ref tc(buf->task);
  if (EXPECT_FALSE(!tc.valid()))
    return nullptr;

  Obj_space::Capability dst_cap = rcv_tsk->lookup(tc.cap());
  if (EXPECT_FALSE(!dst_cap.valid()))
    return nullptr;

  Task *dst_tsk = cxx::dyn_cast<Task*>(dst_cap.obj());
  auto task_rights = L4_fpage::Rights(dst_cap.rights());
  if (EXPECT_FALSE(!dst_tsk || !(task_rights & L4_fpage::Rights::CW())))
    return nullptr;

  return dst_tsk;
}

PRIVATE static
bool
Thread::transfer_msg_items(L4_msg_tag const &tag, Thread* snd, Utcb *snd_utcb,
                           Thread *rcv, Utcb *rcv_utcb,
                           L4_fpage::Rights rights)
{
  L4_buf_iter mem_buffer(rcv_utcb, rcv_utcb->buf_desc.mem());
  L4_buf_iter io_buffer(rcv_utcb, rcv_utcb->buf_desc.io());
  L4_buf_iter obj_buffer(rcv_utcb, rcv_utcb->buf_desc.obj());
  L4_snd_item_iter snd_item(snd_utcb, tag.words());
  int items = tag.items();
  Mword *rcv_word = rcv_utcb->values + tag.words();

  // Must be destroyed _after_ releasing the existence lock below!
  Reap_list reap_list;

  while (items > 0 && snd_item.next())
    {
      L4_snd_item_iter::Item const *const item = snd_item.get();

      if (item->b.is_void())
        { // XXX: not sure if void fpages are needed
          // skip send item and current rcv_buffer
          --items;
          *rcv_word = 0;
          rcv_word += 2;
          continue;
        }

      L4_buf_iter *buf_iter = nullptr;

      switch (item->b.type())
        {
        case L4_msg_item::Map:
          switch (L4_fpage(item->d).type())
            {
            case L4_fpage::Memory: buf_iter = &mem_buffer; break;
            case L4_fpage::Io:     buf_iter = &io_buffer; break;
            case L4_fpage::Obj:    buf_iter = &obj_buffer; break;
            default: break;
            }
          break;
        default:
          break;
        }

      if (EXPECT_FALSE(!buf_iter))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

      L4_buf_iter::Item const *const buf = buf_iter->get();

      if (EXPECT_FALSE(buf->b.is_void() || buf->b.type() != item->b.type()))
        {
          snd->set_ipc_error(L4_error::Overflow, rcv);
          return false;
        }

        {
          assert (item->b.type() == L4_msg_item::Map);
          L4_fpage sfp(item->d);
          L4_return_item_writer return_item(rcv_word, item->b, sfp);

          rcv_word += 2;

          // diminish when sending via restricted IPC gates
          if (sfp.type() == L4_fpage::Obj)
            sfp.mask_rights(rights | L4_fpage::Rights::CRW() | L4_fpage::Rights::CD());

          if (!try_transfer_local_id(buf, sfp, return_item, snd, rcv))
            {
              // we need to do a real mapping
              L4_error err;
              Ref_ptr<Task> dst_tsk(transfer_msg_lookup_dst_tsk(rcv, buf));
              if (EXPECT_FALSE(!dst_tsk))
                {
                  snd->set_ipc_error(L4_error::Overflow, rcv);
                  return false;
                }

              {
                // Take the existence_lock for synchronizing maps -- kind of
                // coarse-grained.
                auto sp_lock = switch_lock_guard(dst_tsk->existence_lock);
                if (!sp_lock.is_valid())
                  {
                    snd->set_ipc_error(L4_error::Overflow, rcv);
                    return false;
                  }

                auto c_lock = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
                err = fpage_map(snd->space(), sfp, dst_tsk.get(),
                                L4_fpage(buf->d), item->b, reap_list.list());
                if (err.empty_map())
                  return_item.set_rcv_type_map_nothing();
              }

              if (EXPECT_FALSE(!err.ok()))
                {
                  snd->set_ipc_error(err, rcv);
                  return false;
                }
            }
        }

      --items;

      if (!item->b.compound())
        buf_iter->next();
    }

  if (EXPECT_FALSE(items))
    {
      snd->set_ipc_error(L4_error::Overflow, rcv);
      return false;
    }

  return true;
}


/**
 * \pre Runs on the sender CPU
 * \retval true when the IPC was aborted
 * \retval false iff the IPC was already finished
 */
PRIVATE inline
bool
Thread::abort_send(L4_error const &e, Thread *partner)
{
  state_del_dirty(Thread_full_ipc_mask);
  Abort_state abt = Abt_ipc_done;

  if (partner->home_cpu() == current_cpu())
    {
      if (in_sender_list())
        {
          sender_dequeue(partner->sender_list());
          partner->vcpu_update_state();
          abt = Abt_ipc_cancel;
        }
      else if (partner->in_ipc(this))
        {
          state_add_dirty(Thread_ipc_transfer);
          abt = Abt_ipc_in_progress;
        }
    }
  else
    abt = partner->Receiver::abort_send(this, this);

  switch (abt)
    {
    default:
    case Abt_ipc_done:
      return false;
    case Abt_ipc_cancel:
      utcb().access()->error = e;
      return true;
    case Abt_ipc_in_progress:
      // In case partner is on current CPU, the Thread_ipc_transfer flag was set
      // above, otherwise in Receiver::abort_send() via xcpu_state_change().
      while (state() & Thread_ipc_transfer)
        {
          state_del_dirty(Thread_ready);
          schedule();
        }
      return false;
    }
}



/**
 * \pre Runs on the sender CPU
 * \retval true iff the IPC was finished during the wait
 * \retval false iff the IPC was aborted with some error
 */
PRIVATE inline
bool
Thread::do_send_wait(Thread *partner, L4_timeout snd_t)
{
  IPC_timeout timeout;

  if (EXPECT_FALSE(snd_t.is_finite()))
    {
      Unsigned64 clock = Timer::system_clock();
      Unsigned64 tval = snd_t.microsecs(clock, utcb().access(true));
      // Zero timeout or timeout expired already -- give up
      if (tval == 0 || tval <= clock)
        return !abort_send(L4_error::Timeout, partner);

      set_timeout(&timeout, tval);
    }

  Mword ipc_state;

  while (((ipc_state = state() & (Thread_send_wait | Thread_ipc_abort_mask))) == Thread_send_wait)
    {
      state_del_dirty(Thread_ready);
      schedule();
    }

  reset_timeout();

  if (EXPECT_TRUE(!(ipc_state & Thread_send_wait)))
    return true;

  if (EXPECT_FALSE(ipc_state & Thread_transfer_failed))
    {
      state_del_dirty(Thread_full_ipc_mask);
      return false;
    }

  if (EXPECT_FALSE(ipc_state & Thread_cancel))
    return !abort_send(L4_error::Canceled, partner);

  if (EXPECT_FALSE(ipc_state & Thread_timeout))
    return !abort_send(L4_error::Timeout, partner);

  return true;
}

PRIVATE inline NOEXPORT
bool
Thread::remote_ipc_send(Ipc_remote_request *rq)
{
  Check_sender r = rq->partner->check_sender(this, rq->zero_timeout);
  switch (r.s)
    {
    case Check_sender::Failed:
      xcpu_state_change(~Thread_ipc_mask, 0);
      rq->result = Check_sender::Failed;
      return false;
    case Check_sender::Queued:
      rq->result = Check_sender::Queued;
      return false;
    default:
      break;
    }

  if (rq->tag.transfer_fpu())
    rq->partner->prepare_xcpu_ipc_transfer_fpu();

  // We may need to grab locks but this is forbidden in a DRQ handler. So
  // transfer the IPC in usual thread code at the sender side. However, this
  // induces an overhead of two extra IPIs.
  if (rq->tag.items())
    {
      xcpu_state_change(~Thread_send_wait, Thread_ready);
      rq->partner->state_change_dirty(~(Thread_ipc_mask | Thread_ready), Thread_ipc_transfer);
      rq->result = r;
      return true;
    }

  bool success = transfer_msg(rq->tag, rq->partner,
                              _ipc_send_rights, r.is_open_wait());
  if (success && rq->have_rcv)
    xcpu_state_change(~Thread_send_wait, Thread_receive_wait);
  else
    xcpu_state_change(~Thread_ipc_mask, 0);

  rq->result = success ? Check_sender::Done : Check_sender::Failed;
  rq->partner->state_change_dirty(~Thread_ipc_mask, Thread_ready);
  if (rq->partner->home_cpu() == current_cpu() && current() != rq->partner)
    Sched_context::rq.current().ready_enqueue(rq->partner->sched());

  return true;
}

PRIVATE static
Context::Drq::Result
Thread::handle_remote_ipc_send(Drq *src, Context *, void *_rq)
{
  Ipc_remote_request *rq = static_cast<Ipc_remote_request*>(_rq);
  bool r = nonull_static_cast<Thread*>(src->context())->remote_ipc_send(rq);
  return r ? Drq::need_resched() : Drq::done();
}

/**
 * \pre Runs on the sender CPU
 */
PRIVATE
Thread::Check_sender
Thread::remote_handshake_receiver(L4_msg_tag const &tag, Thread *partner,
                                  bool have_receive, L4_timeout snd_t)
{
  Ipc_remote_request rq;
  rq.tag = tag;
  rq.have_rcv = have_receive;
  rq.partner = partner;
  rq.zero_timeout = snd_t.is_zero();

  set_wait_queue(partner->sender_list());

  state_add_dirty(Thread_send_wait);

  if (tag.transfer_fpu())
    spill_fpu_if_owner();

  partner->drq(handle_remote_ipc_send, &rq);

  return rq.result;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"

IMPLEMENT
void
Thread::Log_pf_invalid::print(String_buffer *buf) const
{
  buf->printf("InvCap C:%lx pfa=%lx err=%lx",
              cxx::int_value<Cap_index>(cap_idx), pfa, err);
}

IMPLEMENT
void
Thread::Log_exc_invalid::print(String_buffer *buf) const
{
  buf->printf("InvCap C:%lx", cxx::int_value<Cap_index>(cap_idx));
}
