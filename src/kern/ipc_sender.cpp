INTERFACE:

#include "sender.h"
#include "receiver.h"

class Ipc_sender_base : public Sender
{
public:
  virtual ~Ipc_sender_base() = 0;
};

template< typename Derived >
class Ipc_sender : public Ipc_sender_base
{
private:
  Derived *derived() { return static_cast<Derived*>(this); }
  static bool dequeue_sender() { return true; }
  static bool requeue_sender() { return false; }
};

extern "C" void fast_ret_from_irq(void);

IMPLEMENTATION:

#include "config.h"
#include "entry_frame.h"
#include "globals.h"
#include "thread_state.h"
#include <cassert>

IMPLEMENT inline Ipc_sender_base::~Ipc_sender_base() {}

PUBLIC
virtual void
Ipc_sender_base::ipc_receiver_aborted()
{
  assert (wait_queue());
  set_wait_queue(0);
}

/** Sender-activation function called when receiver gets ready.
    Irq::hit() actually ensures that this method is always called
    when an interrupt occurs, even when the receiver was already
    waiting.
 */
PUBLIC template< typename Derived >
virtual void
Ipc_sender<Derived>::ipc_send_msg(Receiver *recv)
{
  derived()->transfer_msg(recv);
  if (derived()->dequeue_sender())
    {
      sender_dequeue(recv->sender_list());
      recv->vcpu_update_state();
    }
}

PROTECTED inline NEEDS["config.h", "globals.h", "thread_state.h"]
bool
Ipc_sender_base::handle_shortcut(Syscall_frame *dst_regs,
                                 Receiver *receiver)
{
  auto &rq = Sched_context::rq.current();

  if (EXPECT_TRUE
      ((current() != receiver
        && rq.deblock(receiver->sched(), current()->sched(), true)
        // avoid race in do_ipc() after Thread_send_in_progress
        // flag was deleted from receiver's thread state
        // also: no shortcut for alien threads, they need to see the
        // after-syscall exception
        && !(receiver->state()
          & (Thread_drq_wait | Thread_ready_mask | Thread_alien))
        && !rq.schedule_in_progress))) // no schedule in progress
    {
      // we don't need to manipulate the state in a safe way
      // because we are still running with interrupts turned off
      receiver->state_add_dirty(Thread_ready);

      if (!Config::Irq_shortcut)
        {
          // no shortcut: switch to the interrupt thread which will
          // calls Irq::ipc_receiver_ready
          current()->switch_to_locked(receiver);
          return true;
        }

      // At this point we are sure that the connected interrupt
      // thread is waiting for the next interrupt and that its 
      // thread priority is higher than the current one. So we
      // choose a short cut: Instead of doing the full ipc handshake
      // we simply build up the return stack frame and go out as 
      // quick as possible.
      //
      // XXX We must own the kernel lock for this optimization!
      //

      Mword *esp = reinterpret_cast<Mword*>(Entry_frame::to_entry_frame(dst_regs));
      receiver->set_kernel_sp(esp);
      receiver->prepare_switch_to(fast_ret_from_irq);

      // directly switch to the interrupt thread context and go out
      // fast using fast_ret_from_irq (implemented in assembler).
      // kernel-unlock is done in switch_exec() (on switchee's side).

      // no shortcut if profiling: switch to the interrupt thread
      current()->switch_to_locked(receiver);
      return true;
    }
  return false;
}


PROTECTED template< typename Derived >
inline  NEEDS["config.h","globals.h", "thread_state.h",
              Ipc_sender_base::handle_shortcut]
bool
Ipc_sender<Derived>::send_msg(Receiver *receiver, bool is_not_xcpu)
{
  set_wait_queue(receiver->sender_list());

  if (!Config::Irq_shortcut)
    {
      // enqueue _after_ shortcut if still necessary
      sender_enqueue(receiver->sender_list(), 255);
      receiver->vcpu_set_irq_pending();
    }

  // if the thread is waiting for this interrupt, make it ready;
  // this will cause it to run irq->receiver_ready(), which
  // handles the rest

  // XXX careful!  This code may run in midst of an do_ipc()
  // operation (or similar)!
  if (Receiver::Rcv_state s = receiver->sender_ok(this))
    {
      Syscall_frame *dst_regs = derived()->transfer_msg(receiver);

      if (derived()->requeue_sender())
	{
	  sender_enqueue(receiver->sender_list(), 255);
	  receiver->vcpu_set_irq_pending();
	}

      // ipc completed
      receiver->state_change_dirty(~Thread_ipc_mask, 0);

      // in case a timeout was set
      receiver->reset_timeout();

      if (is_not_xcpu
          || EXPECT_TRUE(current_cpu() == receiver->home_cpu()))
        {
          auto &rq = Sched_context::rq.current();
          if (s == Receiver::Rs_ipc_receive
              && handle_shortcut(dst_regs, receiver))
            return false;

          // we don't need to manipulate the state in a safe way
          // because we are still running with interrupts turned off
          receiver->state_add_dirty(Thread_ready);
          return rq.deblock(receiver->sched(), current()->sched(), false);
        }

      // receiver's CPU is offline ----------------------------------------
      auto &rq = Sched_context::rq.cpu(receiver->home_cpu());
      // we don't need to manipulate the state in a safe way
      // because we are still running with interrupts turned off
      receiver->state_add_dirty(Thread_ready);
      rq.deblock_refill(receiver->sched());
      rq.ready_enqueue(receiver->sched());
      return false;
    }

  if (Config::Irq_shortcut)
    {
      // enqueue after shortcut if still necessary
      sender_enqueue(receiver->sender_list(), 255);
      receiver->vcpu_set_irq_pending();
    }
  return false;
}

