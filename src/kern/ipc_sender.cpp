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

  // Derived has to implement a finish_send() function, which Ipc_sender calls
  // when it transferred the IPC message or the IPC send was aborted by the
  // receiver. Ipc_sender must assume that finish_send() deletes this kernel
  // object, i.e. must not use this or derived() afterwards.
  /* void finish_send(); */
};

extern "C" void fast_ret_from_irq(void);

IMPLEMENTATION:

#include "config.h"
#include "entry_frame.h"
#include "globals.h"
#include "thread_state.h"
#include <cassert>

IMPLEMENT inline Ipc_sender_base::~Ipc_sender_base() {}

PUBLIC template< typename Derived >
virtual void
Ipc_sender<Derived>::ipc_receiver_aborted() override
{
  assert (wait_queue());

  set_wait_queue(nullptr);
  derived()->finish_send(); // WARN: Do not use this/derived() from here on!
}

PUBLIC template< typename Derived >
virtual void
Ipc_sender<Derived>::ipc_send_msg(Receiver *receiver, bool) override
{
  sender_dequeue(receiver->sender_list());
  receiver->on_sender_dequeued(this);

  derived()->transfer_msg(receiver);

  derived()->finish_send(); // WARN: Do not use this/derived() from here on!

}

PROTECTED inline NEEDS["config.h", "globals.h", "thread_state.h"]
bool
Ipc_sender_base::handle_shortcut(Syscall_frame *dst_regs, Receiver *receiver)
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
          & (Thread_drq_wait | Thread_ready_mask | Thread_alien
             | Thread_switch_hazards))
        && !rq.schedule_in_progress))) // no schedule in progress
    {
      // we don't need to manipulate the state in a safe way
      // because we are still running with interrupts turned off
      receiver->state_add_dirty(Thread_ready);

      if constexpr (!Config::Irq_shortcut)
        {
          // no shortcut: switch to the interrupt thread which will, as for
          // regular IPC, continue from Thread::do_receive_wait().
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
Ipc_sender<Derived>::send_msg(Receiver *receiver, bool is_xcpu)
{
  // XXX careful!  This code may run in midst of an do_ipc()
  // operation (or similar)!
  if (Receiver::Rcv_state s = receiver->sender_ok(this))
    {
      // Receiver is ready to receive this interrupt immediately.
      Syscall_frame *dst_regs = derived()->transfer_msg(receiver);

      derived()->finish_send(); // WARN: Do not use this/derived() from here on!

      // ipc completed
      receiver->state_change_dirty(~Thread_ipc_mask, 0);

      // in case a timeout was set
      receiver->reset_timeout();

      if (!is_xcpu
          || EXPECT_TRUE(current_cpu() == receiver->home_cpu()))
        {
          auto &rq = Sched_context::rq.current();
          if (s.is_ipc() // Shortcut is applicable with receiver that is in
                         // explicit IPC wait (Thread_receive_wait flag set),
                         // not asynchronous vCPU reception.
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
  else
    {
      // Enqueue if receiver is currently not ready to receive the interrupt.
      set_wait_queue(receiver->sender_list());
      sender_enqueue(receiver->sender_list(), 255);
      receiver->on_sender_enqueued(this);
      return false;
    }
}

