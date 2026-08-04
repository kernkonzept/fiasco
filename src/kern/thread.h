#pragma once

#include <cxx/atomic>

#include "l4_types.h"
#include "config.h"
#include "continuation.h"
#include "helping_lock.h"
#include "irq_chip.h"
#include "kobject.h"
#include "mem_layout.h"
#include "member_offs.h"
#include "context.h"
#include "ref_obj.h"
#include "sender.h"
#include "spin_lock.h"

#include <kmem_alloc.h>
#include <task.h>
#include <kernel_task.h>
#include <irq_chip.h>
#include <sched.h>
#include <system_clock.h>
#include <thread_ipc.h>
#include <thread_arch.h>

#include <globalconfig.h>

#ifdef CONFIG_FPU
#  ifdef CONFIG_LAZY_FPU
#    include <thread_lazy_fpu.h>
#  else // CONFIG_LAZY_FPU
#    include <thread_eager_fpu.h>
#  endif // CONFIG_LAZY_FPU
#else // CONFIG_FPU
#  include <thread_no_fpu.h>
#endif // CONFIG_FPU

class Return_frame;
class Syscall_frame;
class Task;
class Thread;
class Vcpu_state;
class Irq_base;

/** A thread.  This class is the driver class for most kernel functionality.
 */
class Thread :
  public Context,
  public Thread_ipc<Thread>,
  public cxx::Dyn_castable<Thread, Kobject>,
  public Thread_fpu_x<Thread>,
  public Thread_arch_x<Thread>,
  public Ref_cnt_obj
{
  MEMBER_OFFSET();

  friend Thread_ipc<Thread>;
  friend Thread_arch_x<Thread>;

  friend class Jdb;
  friend class Jdb_bt;
  friend class Jdb_tcb;
  friend class Jdb_thread;
  friend class Jdb_thread_list;
  friend class Jdb_list_threads;
  friend class Jdb_list_timeouts;
  friend class Jdb_tbuf_show;
  friend struct Utest; // do_leave_and_kill_myself()
  friend class Scheduler_test; // do_leave_and_kill_myself()

public:
  enum Context_mode_kernel { Kernel = 0 };
  enum Operation
  {
    Opcode_mask = 0xffff,
    Op_control = 0,
    Op_ex_regs = 1,
    Op_switch  = 2,
    Op_stats   = 3,
    Op_vcpu_resume = 4,
    Op_register_del_irq = 5,
    Op_modify_senders = 6,
    Op_vcpu_control = 7,
    Op_gdt_x86 = 0x10,
    Op_set_tpidruro_arm = 0x10,
    Op_set_segment_base_amd64 = 0x12,
    Op_segment_info_amd64 = 0x13,
  };

  enum Control_flags
  {
    Ctl_set_pager       = 0x0010000,
    Ctl_bind_task       = 0x0200000,
    Ctl_set_exc_handler = 0x1000000,
  };

  enum Ex_regs_flags
  {
    Exr_cancel            = 0x10000,
    Exr_trigger_exception = 0x20000,
  };

  enum Vcpu_ctl_flags
  {
    Vcpu_ctl_extended_vcpu = 0x10000,
  };

public:
  typedef void (Utcb_copy_func)(Thread *sender, Thread *receiver);

  void *operator new(size_t, Ram_quota *q) noexcept
  {
    void *t = Kmem_alloc::allocator()->q_alloc(q, Bytes(Thread::Size));

    return t;
  }

  // construct a thread
  explicit Thread(Ram_quota *);

  virtual ~Thread() noexcept = 0;

  Ram_quota *quota() const
  {
    return _quota;
  }

  void set_vcpu_user_space(Task *t)
  {
    assert (current() == this);
    if (t)
      t->inc_ref();

    Task *old = static_cast<Task*>(_cpu_state.space.vcpu_user());
    _cpu_state.space.vcpu_user(t);

    if (old && !old->dec_ref())
      delete old;
  }

  // bind a thread to a kernel task t
  void kbind(Task *t)
  {
    auto guard = lock_guard(_cpu_state.space.lock());
    _cpu_state.space.space(t);
    t->inc_ref();
  }

  // bind the thread to a task (incl. UTCB)
  bool bind(Task *t, User_ptr<Utcb> utcb);

  // unbind the thread from its task
  void unbind();

  // register an IRQ to be triggered for IPC gate deletion
  bool register_delete_irq(Irq_base *irq);

  // unregister the del IRQ
  void unregister_delete_irq();

  void remove_delete_irq(Irq_base *irq)
  {
    _del_observer.compare_exchange_strong(irq, (Irq_base *)nullptr);
  }

  void ipc_gate_deleted(Mword id)
  {
    (void) id;
    auto g = lock_guard(cpu_lock);
    if (auto irq = _del_observer.load(cxx::memory_order_relaxed))
      irq->hit(0);
  }


  void ipc_receiver_aborted() override;

  bool exception_triggered() const
  {
    return _exc_cont.valid(regs());
  }

  Mword user_ip() const
  { return _exc_cont.cond_ip(regs()); }

  void user_ip(Mword ip)
  { _exc_cont.cond_ip(regs(), ip); }

  Mword user_sp() const
  { return _exc_cont.cond_sp(regs()); }

  void user_sp(Mword sp)
  { _exc_cont.cond_sp(regs(), sp); }

  bool continuation_test_and_restore()
  {
    bool v = _exc_cont.valid(regs());
    if (v)
      _exc_cont.restore(regs());
    return v;
  }

  void handle_timer_interrupt()
  {
    Cpu_number _cpu = current_cpu();

    static_assert(!(Config::Scheduler_one_shot && !Config::Fine_grained_cputime),
                  "One-shot timer mode requires fine-grained CPU time.");
    // This assumes periodic timers, i.e. does not work for one-shot mode.
    if (!Config::Fine_grained_cputime)
      consume_time(Config::Scheduler_granularity);

    bool resched = Rcu::do_pending_work(_cpu);

    // Check if we need to reschedule due to timeouts or wakeups
    Unsigned64 now = System_clock::clock();
    if ((Timeout_q::timeout_queue.cpu(_cpu).do_timeouts(now) || resched)
        && !Sched_context::rq.current().schedule_in_progress)
      {
        schedule();
        assert (timeslice_timeout.cpu(current_cpu())->is_set());	// Coma check
      }
  }

  static Drq::Result handle_kill_helper(Drq *src, Context *, void *);
  static Drq::Result handle_migration_helper(Drq *rq, Context *, void *p);

  bool kill();

  long control(Thread_ptr const &pager, Thread_ptr const &exc_handler);

  bool check_sys_ipc(unsigned flags, Thread **partner, Thread **sender,
                     bool *have_recv) const
  {
    if (flags & L4_obj_ref::Ipc_recv)
      {
        if (!(flags & L4_obj_ref::Ipc_open_wait))
          *sender = const_cast<Thread*>(this);

        *have_recv = true;
      }

    if (flags & L4_obj_ref::Ipc_send)
      *partner = const_cast<Thread*>(this);

    return *have_recv || ((flags & L4_obj_ref::Ipc_send) && *partner);
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
  bool setup_timer(L4_timeout timeout, Utcb const *utcb, Timeout *timer)
  {
    if (EXPECT_TRUE(timeout.is_never()))
      return true;

    if (EXPECT_FALSE(timeout.is_zero()))
      {
        state.add_dirty(Thread_ready | Thread_timeout);
        return false;
      }

    assert (!have_timeout());

    Unsigned64 sysclock = System_clock::clock();
    Unsigned64 tval = timeout.microsecs(sysclock, utcb);

    if (EXPECT_TRUE((tval > sysclock)))
      {
        set_timeout(timer, tval);
        return true;
      }
    else // timeout already hit
      {
        state.add_dirty(Thread_ready | Thread_timeout);
        return false;
      }
  }


#ifdef CONFIG_JDB
  void halt();
#endif

  // --- static fns -----
  static void assert_irq_entry()
  {
    assert(Sched_context::rq.current().schedule_in_progress
           || current()->state.has(  Thread_ready_mask
                                   | Thread_drq_wait
                                   | Thread_waiting));
  }

  [[noreturn]] static void system_abort();

private:
  void *operator new(size_t);	///< Default new operator undefined

  bool do_kill();

  /**
   * Return to user.
   *
   * This function is the default routine run if a newly
   * initialized context is being switch_exec()'ed.
   */
  [[noreturn]] static void user_invoke();

  static void do_leave_and_kill_myself() asm("thread_do_leave_and_kill_myself");
  static Drq::Result handle_remote_kill(Drq *, Context *self, void *);

  static void user_invoke_generic()
  {
    Context *const c = current();
    assert (c->state() & Thread_ready_mask);

    if (c->handle_drq())
      c->schedule();

    // release CPU lock explicitly, because
    // * the context that switched to us holds the CPU lock
    // * we run on a newly-created stack without a CPU lock guard
    cpu_lock.clear();
  }

  void prepare_kill()
  {
    [[noreturn]] extern void leave_and_kill_myself() asm ("leave_and_kill_myself");

    if (state() & (Thread_dying | Thread_dead))
      return;

    inc_ref();
    state.add(Thread_dying | Thread_cancel | Thread_ready);
    _exc_cont.restore(regs()); // overwrite an already triggered exception
    do_trigger_exception(regs(), (void*)&leave_and_kill_myself);
  }

public:
  /** nesting level in debugger (always critical) if >1 */
  static Per_cpu<unsigned long> nested_trap_recover;
  static void handle_global_remote_requests_irq() asm ("ipi_remote_call");

protected:
  /**
   * Cut-down version of Thread constructor; only for kernel threads
   * Do only what's necessary to get a kernel thread started --
   * skip all fancy stuff, no locking is necessary.
   */
  explicit Thread(Ram_quota *q, Context_mode_kernel)
  : _quota(q)
  {
    inc_ref();
    _cpu_state.space.space(Kernel_task::kernel_task());

    if (Config::Stack_depth)
      std::memset((char*)this + sizeof(Thread), '5',
                  Thread::Size-sizeof(Thread) - 64);

    alloc_eager_fpu_state();
  }

  int
  do_trigger_exception(Entry_frame *r, void *ret_handler)
  {
    if (_exc_cont.valid(r))
      return 0;

    _exc_cont.activate(r, ret_handler);
    return 1;
  }


protected:
  Ram_quota *_quota{nullptr};
  cxx::atomic<Irq_base *> _del_observer{nullptr};

  constexpr static unsigned magic = 0xf001c001;
  // Debugging facilities
  unsigned _magic = magic;

public:
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};

/** Currently executing thread.
    @return currently executing thread.
 */
inline
Thread*
current_thread()
{ return nonull_static_cast<Thread*>(current()); }

