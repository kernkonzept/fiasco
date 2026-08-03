#pragma once

#include "sender.h"
#include "context.h"
#include "config.h"
#include "globals.h"
#include "thread_state.h"

#include <cassert>

extern "C" void fast_ret_from_irq(void);

class Ipc_sender_base : public Sender
{
public:
  virtual ~Ipc_sender_base() = 0;

  bool handle_shortcut(Syscall_frame *dst_regs,
                       Context *receiver, Mword rcv_state)
  {
    auto &rq = Sched_context::rq.current();

    if (EXPECT_TRUE
        ((current() != receiver
          && rq.deblock(receiver->sched(), current()->sched(), true)
          // avoid race in do_ipc() after Thread_send_in_progress
          // flag was deleted from receiver's thread state
          // after-syscall exception
          && !(rcv_state
            & (Thread_drq_wait | Thread_ready_mask
               | Thread_switch_hazards))
          && !rq.schedule_in_progress))) // no schedule in progress
      {
        if constexpr (!Config::Irq_shortcut)
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

};

template< typename Derived >
class Ipc_sender : public Ipc_sender_base
{
private:
  Derived *derived() { return static_cast<Derived*>(this); }
  static bool requeue_sender() { return false; }

public:
  /**
   * Receiver-ready callback. Receivers call this function in the context of a
   * waiting sender when they get ready to receive a message from that sender (in
   * this case an Ipc_sender aka Irq_sender).
   */
  void ipc_send_msg(Context *receiver, Sender *) override
  {
    derived()->transfer_msg(receiver);
    if (derived()->requeue_sender())
      {
        sender_enqueue(receiver->sender_list(), 255);
        receiver->vcpu_set_irq_pending();
      }
  }

  void ipc_receiver_aborted() override
  {
    // nothing actively to stop here
  }

protected:
  bool send_msg(Context *receiver, bool cpu_local_receiver)
  {
    auto s = check_sender(receiver, cpu_local_receiver);
    if (EXPECT_FALSE(!s))
      return false;

    // in case a timeout was set
    if (cpu_local_receiver)
      receiver->reset_timeout();

    Syscall_frame *dst_regs = derived()->transfer_msg(receiver);

    if (derived()->requeue_sender())
      {
        sender_enqueue(receiver->sender_list(), 255);
        receiver->vcpu_set_irq_pending();
      }

    // ipc completed
    receiver->state.change(~Thread_ipc_mask, Thread_ready);

    if (!cpu_local_receiver)
      {
        receiver->remote_ready_enqueue();
        return false;
      }

    if (s.is_ipc()
        && handle_shortcut(dst_regs, receiver, receiver->state()))
      return false;

    auto &rq = Sched_context::rq.current();
    return rq.deblock(receiver->sched(), current()->sched(), false);
  }

private:
  Context::Rcv_state
  check_sender(Context *receiver, bool cpu_local)
  {
#if 0
    if (EXPECT_FALSE(receiver->is_invalid()))
      return Context::Rcv_state::Not_receiving;
#endif

    if (auto ok = receiver->sender_ok(this, cpu_local))
      if (receiver->state.change_safely(~Thread_receive_wait, Thread_receive_in_progress))
        return ok;

    for (;;)
      {
        check (sender_enqueue(receiver->sender_list(), 255));

        if (auto ok = receiver->sender_ok(this, cpu_local))
          {
            if (!sender_dequeue(receiver->sender_list()))
              return Context::Rcv_state::Not_receiving;

            if (receiver->state.change_safely(~Thread_receive_wait, Thread_receive_in_progress))
              return ok;
          }
        else
          break;
      }

    receiver->vcpu_set_irq_pending();
    if (!cpu_local)
      receiver->set_xcpu_ipc_pending();
    return Context::Rcv_state::Not_receiving;
  }

};


inline Ipc_sender_base::~Ipc_sender_base() {}

