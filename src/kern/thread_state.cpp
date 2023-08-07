INTERFACE:

enum Thread_state
{
  /// TCB unallocated
  Thread_invalid          = 0,
  /// Thread can be scheduled.
  Thread_ready            = 0x1,
  /// DRQ pending for this context.
  Thread_drq_ready        = 0x2,
  Thread_ready_mask       = Thread_ready | Thread_drq_ready,

  /// Waiting to send a message.
  Thread_send_wait           = 0x4,
  /// Waiting for a message.
  Thread_receive_wait        = 0x8,
  /// Actively receiving a message. A thread is carrying this flag while
  /// performing the IPC transfer operation to itself in the context of the
  /// next sender.
  Thread_receive_in_progress = 0x10,

  Thread_ipc_mask            = Thread_send_wait | Thread_receive_wait
                             | Thread_receive_in_progress,

  /// Passively receiving a message until this flag is cleared.
  Thread_ipc_transfer        = 0x20,

  /// The IPC operation is canceled by the receiver.
  Thread_transfer_failed      = 0x40,
  /// State has been changed -- cancel activity.
  Thread_cancel               = 0x80,
  /// IPC timeout hit. Either expired, or the timeout is zero and there is no
  /// sender yet at the IPC receive phase.
  Thread_timeout              = 0x100,

  Thread_full_ipc_mask        = Thread_ipc_mask | Thread_cancel | Thread_transfer_failed
                                | Thread_timeout | Thread_ipc_transfer,

  /// If any of these flags is set, the IPC sender will stop waiting for the
  /// receiver.
  Thread_ipc_abort_mask       = Thread_transfer_failed | Thread_cancel | Thread_timeout
                                | Thread_ipc_transfer,

  /// TCB allocated, but inactive (not in any queue).
  Thread_dead                 = 0x200,
  /// Thread is about to be killed.
  Thread_dying                = 0x400,

  // 0x800 is free

  /// Thread::finish_migration must be executed on the new CPU core before
  /// executing any userland code (actually to re-enqueue timeouts).
  Thread_finish_migration     = 0x1000,
  Thread_need_resched         = 0x2000,
  Thread_switch_hazards       = Thread_finish_migration | Thread_need_resched,

  // 0x4000 is free

  /// Thread currently owns the FPU.
  Thread_fpu_owner            = 0x8000,
  /// Thread is an alien -- not allowed to do system calls.
  Thread_alien                = 0x10000,
  /// Thread is an alien, however, the next system call is allowed.
  Thread_dis_alien            = 0x20000,
  /// Thread has sent an exception but still got no reply.
  Thread_in_exception         = 0x40000,

  // 0x80000 is free

  /// Thread polls for DRQs.
  Thread_drq_wait             = 0x100000,
  /// Thread waits for a lock.
  Thread_waiting              = 0x200000,

  /// vCPU state enabled.
  Thread_vcpu_enabled         = 0x400000,
  /// Thread runs currently in vCPU "user" mode with a dedicated user space.
  /// This flag is clear when the thread runs in vCPU "kernel" mode.
  Thread_vcpu_user            = 0x800000,
  /// This thread running in vCPU "user" has no access to the FPU. Any FPU
  /// operation will trigger a corresponding FPU fault.
  Thread_vcpu_fpu_disabled    = 0x1000000,
  /// extended vCPU state enabled. This includes Thread_vcpu_enabled.
  Thread_ext_vcpu_enabled     = 0x2000000,

  // 0x4000000 used by MIPS

  Thread_vcpu_state_mask      = Thread_vcpu_enabled | Thread_vcpu_user
                                | Thread_vcpu_fpu_disabled
                                | Thread_ext_vcpu_enabled
};
