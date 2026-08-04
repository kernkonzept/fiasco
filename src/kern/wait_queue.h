#pragma once

#include "kobject_helper.h"
#include "prio_list.h"
#include "ref_obj.h"
#include "kmem_slab.h"

class Wait_queue final :
  public cxx::Dyn_castable<Wait_queue, Kobject_h<Wait_queue, Kobject>>,
  public Ref_cnt_obj
{
  Wait_queue() = delete;

public:
  explicit Wait_queue(Ram_quota *q, Space *space) : _quota(q), _space(space)
  {
    inc_ref();
  }

  Space *space() const { return _space; }

  void invoke(L4_obj_ref self, L4_fpage::Rights rights,
              Syscall_frame *f, Utcb *utcb) override;

  // Non-virtual send entry point for use by Ipc_gate_dispatch and invoke.
  // Only handles the send (or call) path — no control protocol, no reply-and-wait.
  void do_send_ipc(Thread *ct, L4_obj_ref self, Syscall_frame *f, Utcb *utcb);

  L4_msg_tag kinvoke(L4_obj_ref, L4_fpage::Rights, Syscall_frame *,
                     Utcb const *, Utcb *)
  { return commit_result(-L4_err::ENosys); }

  void destroy(Kobject ***reap_list) override;

  void operator delete (void *ptr);

private:
  friend class Jdb_kobject;

  // Single queue used for either blocked senders or blocked receivers.
  // The two sides are mutually exclusive: a new arrival always checks first
  // whether the other side is waiting and dispatches immediately if so.
  // _has_senders distinguishes which side is currently queued.
  inline Thread *dequeue_receiver();
  inline Sender *dequeue_sender();
  inline Thread *enqueue_sender(Thread *ct, unsigned short prio);
  inline Sender *enqueue_receiver(Thread *ct, unsigned short prio);
  inline void do_receive(Thread *ct, Syscall_frame *f, Utcb *utcb);

  Locked_prio_list _wait_q;
  bool _has_senders = false;
  Ram_quota *_quota;
  Space *_space;
};
