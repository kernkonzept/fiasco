INTERFACE:

enum Thread_state
{
  Thread_invalid          = 0,	  // tcb unallocated
  Thread_ready            = 0x1,  // can be scheduled
  Thread_drq_ready        = 0x2,
  Thread_ready_mask       = Thread_ready | Thread_drq_ready,

  Thread_send_wait           = 0x4,  // waiting to send a message
  Thread_receive_wait        = 0x8,  // waiting for a message
  Thread_receive_in_progress = 0x10, // receiving a message (passively)

  Thread_ipc_mask            = Thread_send_wait | Thread_receive_wait
                             | Thread_receive_in_progress,

  Thread_ipc_transfer        = 0x20,

  // the ipc handshake bits needs to be in the first byte,
  // the shortcut depends on it

  Thread_cancel               = 0x80,// state has been changed -- cancel activity
  Thread_timeout              = 0x100,

  Thread_full_ipc_mask        = Thread_ipc_mask | Thread_cancel
                              | Thread_timeout | Thread_ipc_transfer,

  Thread_ipc_abort_mask       = Thread_cancel | Thread_timeout | Thread_ipc_transfer,

  Thread_dead                 = 0x200,// tcb allocated, but inactive (not in any q)
  Thread_suspended            = 0x400,// thread must not execute user code

  Thread_finish_migration     = 0x1000,
  Thread_need_resched         = 0x2000,
  Thread_switch_hazards       = Thread_finish_migration | Thread_need_resched,

  // 0x800, 0x1000 are free
  //Thread_delayed_deadline     = 0x2000, // delayed until periodic deadline
  //Thread_delayed_ipc          = 0x4000, // delayed until periodic ipc event
  Thread_fpu_owner            = 0x8000, // currently owns the fpu
  Thread_alien                = 0x10000, // Thread is an alien, is not allowed
                                     // to do system calls
  Thread_dis_alien            = 0x20000, // Thread is an alien, however the next
                                     // system call is allowed
  Thread_in_exception         = 0x40000, // Thread has sent an exception but still
                                     // got no reply

  Thread_drq_wait             = 0x100000, // Thread polls for DRQs
  Thread_waiting              = 0x200000, // Thread waits for a lock

  Thread_vcpu_enabled         = 0x400000,
  Thread_vcpu_user            = 0x800000,
  Thread_vcpu_fpu_disabled    = 0x1000000,
  Thread_ext_vcpu_enabled     = 0x2000000,

  Thread_vcpu_state_mask      = Thread_vcpu_enabled | Thread_vcpu_user
                                | Thread_vcpu_fpu_disabled
                                | Thread_ext_vcpu_enabled
};
