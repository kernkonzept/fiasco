INTERFACE:

#include "l4_types.h"
#include "config.h"
#include "continuation.h"
#include "helping_lock.h"
#include "irq_chip.h"
#include "kobject.h"
#include "mem_layout.h"
#include "member_offs.h"
#include "receiver.h"
#include "ref_obj.h"
#include "sender.h"
#include "spin_lock.h"

class Return_frame;
class Syscall_frame;
class Task;
class Thread;
class Vcpu_state;
class Irq_base;

typedef Context_ptr_base<Thread> Thread_ptr;

/** A thread.  This class is the driver class for most kernel functionality.
 */
class Thread :
  public Receiver,
  public Sender,
  public cxx::Dyn_castable<Thread, Kobject>
{
  MEMBER_OFFSET();

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
  enum { Opcode_mask = 0xffff };
  enum class Op : Mword
  {
    Control = 0,
    Ex_regs = 1,
    Switch  = 2,
    Stats   = 3,
    Vcpu_resume = 4,
    Register_del_irq = 5,
    Modify_senders = 6,
    Vcpu_control = 7,
    Register_doorbell_irq = 8,
    Gdt_x86 = 0x10,
    Set_tpidruro_arm = 0x10,
    Set_segment_base_amd64 = 0x12,
    Segment_info_amd64 = 0x13,
  };

  enum Control_flags
  {
    Ctl_set_pager       = 0x0010000,
    Ctl_bind_task       = 0x0200000,
    Ctl_alien_thread    = 0x0400000,
    Ctl_ux_native       = 0x0800000,
    Ctl_set_exc_handler = 0x1000000,
  };

  enum Ex_regs_flags
  {
    Exr_cancel            = 0x10000,
    Exr_trigger_exception = 0x20000,
    Exr_arch_mask         = 0xffUL << 24,
  };

  enum Vcpu_ctl_flags
  {
    Vcpu_ctl_extended_vcpu = 0x10000,
  };

public:
  typedef void (Utcb_copy_func)(Thread *sender, Thread *receiver);

  /**
   * Constructor.
   *
   * @post state() != 0.
   */
  explicit Thread(Ram_quota *);

  int handle_page_fault(Address pfa, Mword error, Mword pc,
                        Return_frame *regs);

private:
  struct Migration_state
  {
    static constexpr Migration_state done_need_resched(){ return Migration_state{1}; }
    static constexpr Migration_state done_no_resched() { return Migration_state{2}; }
    static Migration_state migrate(Migration *migration)
    {
      Mword migration_addr = reinterpret_cast<Mword>(migration);
      assert(!(migration_addr & 0x3)); // ensure alignment
      return Migration_state{migration_addr};
    }

    constexpr bool is_done() const { return _state & 3; }

    // Only valid if is_done() is true.
    constexpr bool need_resched() const { return _state & 1; }

    // Only valid if is_done() is false.
    Migration *migration() const { return reinterpret_cast<Migration *>(_state); }

  private:
    explicit constexpr Migration_state(Mword state) : _state(state) {}

    Mword _state;
  };

  struct Migration_helper_info
  {
    Migration *inf;
    Thread *victim;
  };

  void *operator new(size_t);	///< Default new operator undefined

  bool handle_sigma0_page_fault (Address pfa);

  /**
   * Return to user.
   *
   * This function is the default routine run if a newly
   * initialized context is being switch_exec()'ed.
   */
  [[noreturn]] static void user_invoke();

  static void do_leave_and_kill_myself() asm("thread_do_leave_and_kill_myself");

public:
  /**
   * Check if the pagefault occurred at a special place: At certain places in
   * the kernel (Mem_layout::read_special_safe()), we want to ensure that a
   * specific address is mapped.
   * The regular case is "mapped", the exception or slow case is "not mapped".
   * The fastest way to check this is to touch into the memory. If there is no
   * mapping for the address we get a pagefault. The pagefault handler sets the
   * carry and/or the zero flag which can be detected by the faulting code.
   *
   * @param regs  Pagefault return frame.
   * @returns False or true whether this was a pagefault at a special region or
   *          not. On true, the return frame got the carry flag and/or the zero
   *          flag set (depending on the architecture).
   */
  static bool pagein_tcb_request(Return_frame *regs);

  inline Mword user_ip() const;
  inline void user_ip(Mword);

  inline Mword user_sp() const;
  inline void user_sp(Mword);

  inline Mword user_flags() const;

  /** nesting level in debugger (always critical) if >1 */
  static Per_cpu<unsigned long> nested_trap_recover;
  static void handle_remote_requests_irq() asm ("handle_remote_cpu_requests");
  static void handle_global_remote_requests_irq() asm ("ipi_remote_call");

  bool arch_ext_vcpu_enabled();

protected:
  explicit Thread(Ram_quota *, Context_mode_kernel);

  bool ex_regs_arch(Mword ops);

  // More ipc state
  Thread_ptr _pager;
  Thread_ptr _exc_handler;

protected:
  Ram_quota *_quota;
  Irq_base *_del_observer;


  // Debugging facilities
  unsigned _magic;
  static const unsigned magic = 0xf001c001;
};

// ------------------------------------------------------------------------
INTERFACE[debug]:

EXTENSION class Thread
{
public:
  class Dbg_stack
  {
  public:
    enum { Stack_size = Config::PAGE_SIZE };
    void *stack_top;
    Dbg_stack();
  };

  static Per_cpu<Dbg_stack> dbg_stack;
};


IMPLEMENTATION:

#include <cassert>
#include <cstdlib>		// panic()
#include "atomic.h"
#include "entry_frame.h"
#include "fpu_alloc.h"
#include "globals.h"
#include "irq_chip.h"
#include "kdb_ke.h"
#include "kernel_task.h"
#include "kmem_alloc.h"
#include "logdefs.h"
#include "map_util.h"
#include "ram_quota.h"
#include "sched_context.h"
#include "space.h"
#include "std_macros.h"
#include "task.h"
#include "thread_state.h"
#include "timeout.h"
#include "global_data.h"

JDB_DEFINE_TYPENAME(Thread,  "\033[32mThread\033[m");
DEFINE_PER_CPU Per_cpu<unsigned long> Thread::nested_trap_recover;


PUBLIC inline
void *
Thread::operator new(size_t, Ram_quota *q) noexcept
{
  void *t = Kmem_alloc::allocator()->q_alloc(q, Bytes(Thread::Size));
  if (t)
    memset(t, 0, sizeof(Thread));

  return t;
}

PUBLIC
void
Thread::kbind(Task *t)
{
  auto guard = lock_guard(_space.lock());
  _space.space(t);
  t->inc_ref();
}

PUBLIC
bool
Thread::bind(Task *t, User_ptr<Utcb> utcb)
{
  // _utcb == 0 for all kernel threads
  Space::Ku_mem const *u = t->find_ku_mem(utcb, sizeof(Utcb));

  if (EXPECT_FALSE(!u))
    return false;

  auto guard = lock_guard(_space.lock());
  if (_space.space() != Kernel_task::kernel_task())
    return false;

  _space.space(t);
  t->inc_ref();

  _utcb.set(utcb, u->kern_addr(utcb));
  arch_setup_utcb_ptr();
  return true;
}


PUBLIC
void
Thread::unbind()
{
  assert(   (!(state() & Thread_dead) && current() == this)
         || ( (state() & Thread_dead) && current() != this));

  Task *old;

    {
      auto guard = lock_guard(_space.lock());

      if (_space.space() == Kernel_task::kernel_task())
        return;

      old = static_cast<Task*>(_space.space());
      _space.space(Kernel_task::kernel_task());

      // switch to a safe page table if the thread is to be going itself
      if (   current() == this
          && Mem_space::current_mem_space(current_cpu()) == old)
        Kernel_task::kernel_task()->switchin_context(old);

      if (old->dec_ref())
        old = nullptr;
    }

  if (old)
    delete old;
}

/** Cut-down version of Thread constructor; only for kernel threads
    Do only what's necessary to get a kernel thread started --
    skip all fancy stuff, no locking is necessary.
 */
IMPLEMENT inline NEEDS["kernel_task.h"]
Thread::Thread(Ram_quota *q, Context_mode_kernel)
  : Receiver(), Sender(), _quota(q), _del_observer(nullptr), _magic(magic)
{
  inc_ref();
  _space.space(Kernel_task::kernel_task());

  if constexpr (Config::Stack_depth)
    std::memset(reinterpret_cast<char*>(this) + sizeof(Thread), '5',
                Thread::Size-sizeof(Thread) - 64);

  alloc_eager_fpu_state();
}


/** Destructor.  Reestablish the Context constructor's precondition.
    @pre state() == Thread_dead
    @pre lock_cnt() == 0
    @post (_kernel_sp == 0)  &&  (* (stack end) == 0)  &&  !exists()
 */
PUBLIC virtual
Thread::~Thread()		// To be called in locked state.
{
  // Thread::do_kill() already unregistered deletion and doorbell IRQs, but in
  // the meantime a new IRQ might have been bound again.
  unregister_delete_irq();
  unregister_doorbell_irq();

  unsigned long *init_sp = reinterpret_cast<unsigned long*>
    (reinterpret_cast<unsigned long>(this) + Size - sizeof(Entry_frame));

  _kernel_sp = nullptr;
  *--init_sp = 0;
  Fpu_alloc::free_state(fpu_state());
  assert (!in_ready_list());
}

// IPC-gate deletion stuff ------------------------------------

/**
 * Fake IRQ Chip class for IPC-gate-delete notifications.
 * This chip uses the IRQ number as thread pointer and implements
 * the bind and unbind functionality.
 */
class Del_irq_chip : public Irq_chip_soft
{
public:
  static Global_data<Del_irq_chip> chip;
};

DEFINE_GLOBAL Global_data<Del_irq_chip> Del_irq_chip::chip;

PUBLIC static inline
Thread *Del_irq_chip::thread(Mword pin)
{ return reinterpret_cast<Thread*>(pin); }

PUBLIC static inline
Mword Del_irq_chip::pin(Thread *t)
{ return reinterpret_cast<Mword>(t); }

PUBLIC inline
void
Del_irq_chip::detach(Irq_base *irq) override
{
  thread(irq->pin())->remove_delete_irq(irq);
  Irq_chip_soft::detach(irq);
}


PUBLIC inline NEEDS["irq_chip.h"]
void
Thread::ipc_gate_deleted(Mword /* id */)
{
  auto g = lock_guard(cpu_lock);
  if (Irq_base *del_observer = access_once(&_del_observer))
    del_observer->hit(nullptr);
}

PUBLIC
bool
Thread::register_delete_irq(Irq_base *irq)
{
  if (_del_observer)
    return false;

  auto g = lock_guard(irq->irq_lock());
  irq->detach();
  Del_irq_chip::chip->bind(irq, reinterpret_cast<Mword>(this));
  if (cas<Irq_base *>(&_del_observer, nullptr, irq))
    return true;

  irq->detach();
  return false;
}

PUBLIC
void
Thread::unregister_delete_irq()
{
  auto g = lock_guard(cpu_lock);
  if (Irq_base *del_observer = access_once(&_del_observer))
    {
      auto g = lock_guard(del_observer->irq_lock());
      del_observer->detach();
    }
}

PUBLIC
void
Thread::remove_delete_irq(Irq_base *irq)
{
  cas<Irq_base *>(&_del_observer, irq, nullptr);
}

// end of: IPC-gate deletion stuff -------------------------------

/** Currently executing thread.
    @return currently executing thread.
 */
inline
Thread*
current_thread()
{ return nonull_static_cast<Thread*>(current()); }

PUBLIC inline
bool
Thread::exception_triggered() const
{ return _exc_cont.valid(regs()); }

PUBLIC inline
bool
Thread::continuation_test_and_restore()
{
  bool v = _exc_cont.valid(regs());
  if (v)
    _exc_cont.restore(regs());
  return v;
}

//
// state requests/manipulation
//

PUBLIC inline NEEDS ["config.h", "timeout.h"]
void
Thread::handle_timer_interrupt()
{
  Cpu_number _cpu = current_cpu();

  static_assert(!(Config::Scheduler_one_shot && !Config::Fine_grained_cputime),
                "One-shot timer mode requires fine-grained CPU time.");
  // This assumes periodic timers, i.e. does not work for one-shot mode.
  if constexpr (!Config::Fine_grained_cputime)
    consume_time(Config::Scheduler_granularity);

  bool resched = Rcu::do_pending_work(_cpu);

  // Check if we need to reschedule due to timeouts or wakeups
  Unsigned64 now = Timer::system_clock();
  if ((Timeout_q::timeout_queue.cpu(_cpu).do_timeouts(now) || resched)
      && !Sched_context::rq.current().schedule_in_progress)
    {
      schedule();
      assert (timeslice_timeout.cpu(current_cpu())->is_set());	// Coma check
    }
}


PRIVATE static inline
void
Thread::user_invoke_generic()
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


IMPLEMENT /* static */
void
Thread::do_leave_and_kill_myself()
{
  current_thread()->do_kill();
#ifdef CONFIG_JDB
  WARN("dead thread scheduled: %lx\n", current_thread()->dbg_id());
#endif
  kdb_ke("DEAD SCHED");
}

PUBLIC static
Context::Drq::Result
Thread::handle_kill_helper(Drq *src, Context *, void *)
{
  Thread *to_delete = static_cast<Thread*>(static_cast<Kernel_drq*>(src)->src);
  assert (!to_delete->in_ready_list());
  if (to_delete->dec_ref() == 0)
    delete to_delete;

  return Drq::no_answer_resched();
}

PRIVATE
bool
Thread::do_kill()
{
  // Since this function is executed as a continuation, interrupts should
  // already be disabled, but it does not hurt to make sure.
  auto guard = lock_guard(cpu_lock);

  //
  // Kill this thread
  //

  // But first prevent it from being woken up by asynchronous events

  {
    // if IPC timeout active, reset it
    reset_timeout();

    Sched_context::Ready_queue &rq = Sched_context::rq.current();

    // Switch to time-sharing scheduling context
    if (sched() != sched_context())
      switch_sched(sched_context(), &rq);

    if (!rq.current_sched() || rq.current_sched()->context() == this)
      rq.set_current_sched(current()->sched());
  }

  // If other threads want to send me IPC messages, abort these operations.
  // IMPORTANT: After the following code block, the cpu lock must not be
  //            released until the Thread_dead flag is set, otherwise new
  //            senders can be enqueued in the sender list of the dying thread.
  {
    while (Sender *s = Sender::cast(sender_list()->first()))
      {
        s->sender_dequeue(sender_list());
        s->ipc_receiver_aborted();
        Proc::preemption_point();
      }
  }

  // Must not be engaged in any IPC send operation at this point.
  assert(!in_sender_list());

  if (utcb().kern())
    utcb().access()->free_marker = Utcb::Free_marker;
  // No UTCB access beyond this point!

  release_fpu_if_owner();

  Vcpu_state *vcpu = vcpu_state().access();
  vcpu_enter_kernel_mode(vcpu);
  vcpu_update_state();

  Task *user_space = static_cast<Task *>(vcpu_user_space());
  if (user_space)
    user_space->cleanup_vcpu(this, vcpu);

  unbind();
  vcpu_set_user_space(nullptr);

  arch_vcpu_ext_shutdown();

  state_change_dirty(~Thread_dying, Thread_dead);

  // dequeue from system queues
  Sched_context::rq.current().ready_dequeue(sched());

  unregister_delete_irq();
  unregister_doorbell_irq();

  rcu_wait();

  state_del_dirty(Thread_ready_mask);

  Sched_context::rq.current().ready_dequeue(sched());

  // make sure this thread really never runs again by migrating it
  // to the 'invalid' CPU forcefully and then switching to the kernel
  // thread for doing the last bits.
  force_to_invalid_cpu();
  kernel_context_drq(handle_kill_helper, nullptr);
  kdb_ke("Im dead");
  return true;
}

PRIVATE inline
void
Thread::prepare_kill()
{
  [[noreturn]] extern void leave_and_kill_myself() asm ("leave_and_kill_myself");

  if (state() & (Thread_dying | Thread_dead))
    return;

  inc_ref();
  state_add_dirty(Thread_dying | Thread_cancel | Thread_ready);
  _exc_cont.restore(regs()); // overwrite an already triggered exception
  do_trigger_exception(regs(), reinterpret_cast<void*>(&leave_and_kill_myself));
}

PRIVATE static
Context::Drq::Result
Thread::handle_remote_kill(Drq *, Context *self, void *)
{
  nonull_static_cast<Thread*>(self)->prepare_kill();
  return Drq::done();
}


PUBLIC
bool
Thread::kill()
{
  auto guard = lock_guard(cpu_lock);

  if (home_cpu() == current_cpu())
    {
      prepare_kill();
      Sched_context::rq.current().deblock(sched(), current()->sched());
      return true;
    }

  drq(Thread::handle_remote_kill, nullptr);

  return true;
}


PUBLIC
void
Thread::set_sched_params(L4_sched_param const *p)
{
  Sched_context *sc = sched_context();

  // this can actually access the ready queue of a CPU that is offline remotely
  Sched_context::Ready_queue &rq = Sched_context::rq.cpu(home_cpu());
  rq.ready_dequeue(sched());

  sc->set(p);
  sc->replenish();

  if (sc == rq.current_sched())
    rq.set_current_sched(sc);

  if (state() & Thread_ready_mask) // maybe we could omit enqueueing current
    rq.ready_enqueue(sched());
}

PUBLIC
long
Thread::control(Thread_ptr const &pager, Thread_ptr const &exc_handler)
{
  if (pager.is_valid())
    _pager = pager;

  if (exc_handler.is_valid())
    _exc_handler = exc_handler;

  return 0;
}

IMPLEMENT_DEFAULT inline
bool
Thread::arch_ext_vcpu_enabled()
{ return false; }

PUBLIC static inline
void
Thread::assert_irq_entry()
{
  assert(Sched_context::rq.current().schedule_in_progress
             || current_thread()->state() & (Thread_ready_mask | Thread_drq_wait | Thread_waiting | Thread_ipc_transfer));
}

IMPLEMENT_DEFAULT inline
bool
Thread::ex_regs_arch(Mword ops)
{
  return (ops & Exr_arch_mask) == 0;
}

// ---------------------------------------------------------------------------

PUBLIC inline
bool
Thread::check_sys_ipc(unsigned flags, Thread **partner, Thread **sender,
                      bool *have_recv) const
{
  if (flags & L4_obj_ref::Ipc_recv)
    {
      *sender = flags & L4_obj_ref::Ipc_open_wait ? nullptr : const_cast<Thread*>(this);
      *have_recv = true;
    }

  if (flags & L4_obj_ref::Ipc_send)
    *partner = const_cast<Thread*>(this);

  return *have_recv || ((flags & L4_obj_ref::Ipc_send) && *partner);
}

/**
 * DRQ helper for migrating the current thread with the help of the kernel idle
 * thread.
 */
PUBLIC static
Context::Drq::Result
Thread::handle_migration_helper(Drq *rq, Context *, void *p)
{
  Migration *inf = reinterpret_cast<Migration *>(p);
  Thread *v = static_cast<Thread*>(static_cast<Kernel_drq*>(rq)->src);
  v->do_migration_not_current(inf);
  return Drq::no_answer_resched();
}

/**
 * Determine if changing the home CPU is required: If not, directly set the new
 * scheduling parameters, otherwise return the migration information to the
 * caller.
 *
 * \retval done_need_resched  No change of home CPU, need to reschedule.
 * \retval done_no_resched  No change of home CPU, no need to reschedule.
 * \retval migrate  Scheduling parameters with change of home CPU.
 */
PRIVATE inline
Thread::Migration_state
Thread::start_migration()
{
  assert(cpu_lock.test());
  Migration *m = _migration;

  if (!m || !cas<Migration *>(&_migration, m, nullptr))
    return Migration_state::done_no_resched();

  if (m->cpu == home_cpu())
    {
      set_sched_params(m->sp);
      Mem::mp_mb();
      write_now(&m->caller_may_return, true);
      return Migration_state::done_need_resched();
    }

  return Migration_state::migrate(m); // need to do real migration
}

/**
 * Migrate this thread, the current executing thread.
 * We need help of the kernel idle thread.
 */
PRIVATE inline
bool
Thread::do_migration_current(Migration *m)
{
  assert (current_cpu() == home_cpu());
  return kernel_context_drq(handle_migration_helper, m);
}

/**
 * Migrate this thread which is currently not executing.
 */
PRIVATE inline
bool
Thread::do_migration_not_current(Migration *m)
{
  Cpu_number target_cpu = access_once(&m->cpu);
  bool resched = migrate_away(m, false);
  resched |= migrate_to(target_cpu);
  return resched;
}

/**
 * Set the scheduling parameters and optionally change the home CPU for this
 * thread running on the same CPU as current_cpu().
 *
 * \retval false No rescheduling required.
 * \retval true Rescheduling required.
 */
PRIVATE
bool
Thread::do_migration()
{
  Migration_state inf = start_migration();
  if (inf.is_done())
    return inf.need_resched(); // already migrated, nothing to do

  spill_fpu_if_owner();

  if (current() == this)
    return do_migration_current(inf.migration());
  else
    return do_migration_not_current(inf.migration());
}

/**
 * Migrate this thread.
 *
 * Called from Context::Pending_rqq::handle_requests() if this thread is not
 * the current executing thread on the remote CPU.
 *
 * \pre this != current()
 *
 * \retval false  No rescheduling required.
 * \retval true  Rescheduling required.
 */
PUBLIC
bool
Thread::initiate_migration() override
{
  assert (current() != this);

  Migration_state inf = start_migration();
  if (inf.is_done())
    return inf.need_resched(); // already migrated, nothing to do

  spill_fpu_if_owner();

  return do_migration_not_current(inf.migration());
}

/**
 * Re-engage a possibly pending IPC receive timeout after the migration has
 * finished.
 */
PUBLIC
void
Thread::finish_migration() override
{ enqueue_timeout_again(); }

IMPLEMENT_DEFAULT inline
bool
Thread::pagein_tcb_request(Return_frame *)
{ return false; }

//---------------------------------------------------------------------------
IMPLEMENTATION [fpu && lazy_fpu]:

#include "fpu.h"
#include "fpu_alloc.h"
#include "fpu_state_ptr.h"

/*
 * Handle FPU trap for this context. Assumes disabled interrupts
 */
PUBLIC inline NEEDS ["fpu_alloc.h","fpu_state_ptr.h"]
int
Thread::switchin_fpu(bool alloc_new_fpu = true)
{
  if (state() & Thread_vcpu_fpu_disabled)
    return 0;

  Fpu &f = Fpu::fpu.current();
  // If we own the FPU, we should never be getting an "FPU unavailable" trap
  assert (f.owner() != this);

  // Allocate FPU state slab if we didn't already have one
  if (!fpu_state().valid()
      && (EXPECT_FALSE(!alloc_new_fpu
                       || !Fpu_alloc::alloc_state(_quota, fpu_state()))))
    return 0;

  // Enable the FPU before accessing it, otherwise recursive trap
  f.enable();

  // Save the FPU state of the previous FPU owner (lazy) if applicable
  if (f.owner())
    f.owner()->spill_fpu();

  // Become FPU owner and restore own FPU state
  f.restore_state(fpu_state().get());

  state_add_dirty(Thread_fpu_owner);
  f.set_owner(this);
  return 1;
}

PUBLIC inline
bool
Thread::alloc_eager_fpu_state()
{ return true; }

PUBLIC inline NEEDS["fpu.h", "fpu_alloc.h"]
void
Thread::transfer_fpu(Thread *to) //, Trap_state *trap_state, Utcb *to_utcb)
{
  if (to->fpu_state().valid())
    Fpu_alloc::free_state(to->fpu_state());

  to->fpu_state().set(fpu_state().get());
  fpu_state().set(nullptr);

  if (home_cpu() != to->home_cpu())
    {
      assert (!(to->state() & Thread_fpu_owner));
      assert (!(state() & Thread_fpu_owner));
      return;
    }

  assert (current() == this || current() == to);

  Fpu &f = Fpu::fpu.current();

  f.disable(); // it will be reenabled in switch_fpu

  if (EXPECT_FALSE(f.owner() == to))
    {
      assert (to->state() & Thread_fpu_owner);

      f.set_owner(nullptr);
      to->state_del_dirty(Thread_fpu_owner);
    }
  else if (f.owner() == this)
    {
      assert (state() & Thread_fpu_owner);

      state_del_dirty(Thread_fpu_owner);

      to->state_add_dirty (Thread_fpu_owner);
      f.set_owner(to);
      if (EXPECT_FALSE(current() == to))
        f.enable();
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [fpu && !lazy_fpu]:

#include "fpu.h"
#include "fpu_alloc.h"
#include "fpu_state_ptr.h"

/*
 * Handle FPU trap for this context. Assumes disabled interrupts
 */
PUBLIC inline
int
Thread::switchin_fpu(bool /* alloc_new_fpu */ = true)
{
  if (state() & Thread_vcpu_fpu_disabled)
    return 0;

  panic("must not see any FPU trap with eager FPU\n");
}

PUBLIC inline NEEDS["fpu.h", "fpu_alloc.h"]
bool
Thread::alloc_eager_fpu_state()
{
  return Fpu_alloc::alloc_state(_quota, fpu_state());
}

PUBLIC inline NEEDS["fpu.h", "fpu_state_ptr.h"]
void
Thread::transfer_fpu(Thread *to) //, Trap_state *trap_state, Utcb *to_utcb)
{
  Fpu_alloc::ensure_compatible_state(to->_quota, to->fpu_state(), fpu_state());

  auto *curr = current();
  if (this == curr)
    Fpu::save_state(to->fpu_state().get());
  else if (curr == to)
    Fpu::restore_state(fpu_state().get());
  else
    Fpu::copy_state(to->fpu_state().get(), fpu_state().get());
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!fpu]:

PUBLIC inline
int
Thread::switchin_fpu(bool /* alloc_new_fpu */ = true)
{
  return 0;
}

PUBLIC inline
bool
Thread::alloc_eager_fpu_state()
{
  return true;
}

PUBLIC inline
void
Thread::transfer_fpu(Thread *)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [!log]:

PUBLIC inline
unsigned Thread::sys_ipc_log(Syscall_frame *)
{ return 0; }

PUBLIC inline
unsigned Thread::sys_ipc_trace(Syscall_frame *)
{ return 0; }

static inline
void Thread::page_fault_log(Address, unsigned, unsigned)
{}

PUBLIC static inline
int Thread::log_page_fault()
{ return 0; }

PUBLIC inline
unsigned Thread::sys_fpage_unmap_log(Syscall_frame *)
{ return 0; }

// ----------------------------------------------------------------------------
IMPLEMENTATION [!mp]:


PRIVATE inline
bool
Thread::migrate_away(Migration *inf, bool remote)
{
  assert (current() != this);
  assert (cpu_lock.test());
  bool resched = false;

  Cpu_number cpu = inf->cpu;

  if (_timeout)
    _timeout->reset();

  if (!remote && home_cpu() == current_cpu())
    {
      auto &rq = Sched_context::rq.current();

      // if we are in the middle of the scheduler, leave it now
      if (rq.schedule_in_progress == this)
        rq.schedule_in_progress = nullptr;

      rq.ready_dequeue(sched());

        {
          // Not sure if this can ever happen
          Sched_context *csc = rq.current_sched();
          if (csc == sched())
            {
              rq.set_current_sched(kernel_context(current_cpu())->sched());
              resched = true;
            }
        }
    }

  Sched_context *sc = sched_context();
  sc->set(inf->sp);
  sc->replenish();
  set_sched(sc);

  state_add_dirty(Thread_finish_migration);
  set_home_cpu(cpu);
  write_now(&inf->caller_may_return, true);
  return resched;
}

PRIVATE inline
bool
Thread::migrate_to(Cpu_number target_cpu)
{
  if (!Cpu::online(target_cpu))
    {
      handle_drq();
      return false;
    }

  bool resched = false;
  if (state() & Thread_ready_mask)
    resched = Sched_context::rq.current().deblock(sched(), current()->sched());

  enqueue_timeout_again();

  return resched;
}

PUBLIC
void
Thread::migrate(Migration *info)
{
  assert (cpu_lock.test());

  LOG_TRACE("Thread migration", "mig", this, Migration_log,
      l->state = state(false);
      l->src_cpu = home_cpu();
      l->target_cpu = info->cpu;
      l->user_ip = regs()->ip();
  );

  _migration = info;
  current()->schedule_if(do_migration());
}

PRIVATE inline NOEXPORT
void
Thread::force_to_invalid_cpu()
{
  // make sure this thread really never runs again by migrating it
  // to the 'invalid' CPU forcefully and then switching to the kernel
  // thread for doing the last bits.
  set_home_cpu(Cpu::invalid());
  handle_drq();
}

//----------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Thread
{
protected:
  struct Migration_log : public Tb_entry
  {
    Mword    state;
    Address  user_ip;
    Cpu_number src_cpu;
    Cpu_number target_cpu;

    void print(String_buffer *) const;
  };
  static_assert(sizeof(Migration_log) <= Tb_entry::Tb_entry_size);
};


// ----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "ipi.h"

/**
 * Thread object interface for setting thread scheduler parameters, optionally
 * with changing the home CPU.
 *
 * \pre CPU lock taken.
 *
 * \param info  Migration information containing the new scheduler parameters
 *              and the target CPU.
 */
PUBLIC
void
Thread::migrate(Migration *info)
{
  assert (cpu_lock.test());

  LOG_TRACE("Thread migration", "mig", this, Migration_log,
      l->state = state(false);
      l->src_cpu = home_cpu();
      l->target_cpu = info->cpu;
      l->user_ip = regs()->ip();
  );
    {
      Migration *old;
      do
        old = _migration;
      while (!cas(&_migration, old, info));
      // flag old migration to be done / stale
      if (old)
        write_now(&old->caller_may_return, true);
    }

  Cpu_number cpu = home_cpu();

  if (current_cpu() == cpu)
    current()->schedule_if(do_migration());
  else
    current()->schedule_if(migrate_xcpu(cpu));

  cpu_lock.clear();
  // FIXME: use monitor & mwait or wfe & sev if available
  while (!access_once(&info->caller_may_return))
    Proc::pause();
  cpu_lock.lock();
}

PRIVATE inline NOEXPORT
void
Thread::force_to_invalid_cpu()
{
  // make sure this thread really never runs again by migrating it
  // to the 'invalid' CPU forcefully.
  Queue &q = Context::_pending_rqq.current();

    {
      auto g = lock_guard(q.q_lock());
      set_home_cpu(Cpu::invalid());
      if (_pending_rq.queued())
        q.dequeue(&_pending_rq);
    }
  handle_drq();
}

IMPLEMENT
void
Thread::handle_remote_requests_irq()
{
  assert (cpu_lock.test());
  // printf("CPU[%2u]: > RQ IPI (current=%p)\n", current_cpu(), current());
  Context *const c = current();
  Ipi::eoi(Ipi::Request, current_cpu());
  //LOG_MSG_3VAL(c, "ipi", c->cpu(), (Mword)c, c->drq_pending());

  // we might have to migrate the currently running thread, and we cannot do
  // this during the processing of the request queue. In this case we get the
  // thread in migration_q and do this here.
  Context *migration_q = nullptr;
  bool resched = _pending_rqq.current().handle_requests(&migration_q);

  resched |= Rcu::do_pending_work(current_cpu());

  if (migration_q)
    resched |= static_cast<Thread*>(migration_q)->do_migration();

  bool on_current_cpu = c->home_cpu() == current_cpu();
  if (on_current_cpu)
    resched |= c->handle_drq();

  if (Sched_context::rq.current().schedule_in_progress)
    {
      if (   (c->state() & Thread_ready_mask)
          && !c->in_ready_list()
          && on_current_cpu)
        Sched_context::rq.current().ready_enqueue(c->sched());
    }
  else if (resched)
    c->schedule();
}

/**
 * Handler for Ipi::Global requests.
 *
 * \note RISC-V and MIPS rely on the fact that this function does not call
 *       Context::schedule()! If that ever changes, Context::schedule() would
 *       have to be performed at the end of the IPI handler.
 */
IMPLEMENT
void
Thread::handle_global_remote_requests_irq()
{
  assert (cpu_lock.test());
  // printf("CPU[%2u]: > RQ IPI (current=%p)\n", current_cpu(), current());
  Ipi::eoi(Ipi::Global_request, current_cpu());
  Cpu_call::handle_global_requests();
}

/**
 * First step of changing the home CPU and setting the scheduling parameters
 * for this thread.
 *
 * Dequeue this context from the old CPU's ready queue, possibly dequeue the
 * pending RQ of this context from the old CPU's RQQ, set the scheduling
 * parameters, and set the home CPU.
 *
 * \pre This thread must not be the currently executing thread.
 * \pre CPU lock taken.
 *
 * \param remote  False if home CPU is the current CPU.
 *                True if home CPU is the *invalid* CPU or an offline CPU.
 *
 * \retval false  No rescheduling required.
 * \retval true  Rescheduling required.
 */
PRIVATE inline
bool
Thread::migrate_away(Migration *inf, bool remote)
{
  assert (check_for_current_cpu());
  assert (current() != this);
  assert (cpu_lock.test());
  bool resched = false;

  if (_timeout)
    {
      _timeout->reset();
    }

    {
      Sched_context::Ready_queue &rq = EXPECT_TRUE(!remote)
                                     ? Sched_context::rq.current()
                                     : Sched_context::rq.cpu(home_cpu());

      // if we are in the middle of the scheduler, leave it now
      if (rq.schedule_in_progress == this)
        rq.schedule_in_progress = nullptr;

      rq.ready_dequeue(sched());

      Sched_context *csc = rq.current_sched();
      if (!remote && csc == sched())
        {
          rq.set_current_sched(kernel_context(current_cpu())->sched());
          resched = true;
        }
    }

  Cpu_number target_cpu = inf->cpu;

    {
      Queue &q = _pending_rqq.current();
      // The queue lock of the current CPU protects the cpu number in
      // the thread

      Lock_guard<Queue::Inner_lock> g;
      // For an invalid/offline CPU, migrate_away() is executed in a DRQ, which
      // already acquired the queue lock of the thread's home CPU.
      if (!remote)
        g = lock_guard(q.q_lock());

      assert (q.q_lock()->test());
      // potentially dequeue from our local queue
      if (_pending_rq.queued())
        check (q.dequeue(&_pending_rq));

      Sched_context *sc = sched_context();
      sc->set(inf->sp);
      sc->replenish();
      set_sched(sc);

      Mem::mp_wmb();

      assert (!in_ready_list());
      assert (!_pending_rq.queued());

      // The migration must be finished on the new CPU core before executing any
      // userland code. This will be done by Context::switch_handle_drq() after
      // the next context switch to this context was performed on the new CPU.
      state_add_dirty(Thread_finish_migration);
      set_home_cpu(target_cpu);
      Mem::mp_mb();
      write_now(&inf->caller_may_return, true);
    }

  return resched;
}

/**
 * Second step of changing the home CPU of this thread.
 *
 * If not already done, enqueue the RQ of this context to the RQQ of the target
 * CPU and, if necessary, signal the remote CPU about new entries in its RQQ.
 * Handled on the remote CPU in Thread::Pending_rqq::handle_requests().
 *
 * \retval false  No rescheduling required.
 * \retval true  Rescheduling required.
 */
PRIVATE inline
bool
Thread::migrate_to(Cpu_number target_cpu)
{
  bool ipi = false;
    {
      Queue &q = _pending_rqq.cpu(target_cpu);
      auto g = lock_guard(q.q_lock());

      if (access_once(&_home_cpu) == target_cpu
          && EXPECT_FALSE(!Cpu::online(target_cpu)))
        {
          handle_drq();
          return false;
        }

      // migrated meanwhile
      if (access_once(&_home_cpu) != target_cpu || _pending_rq.queued())
        return false;

      if (target_cpu == current_cpu())
        {
          g.reset();
          bool resched = handle_drq();
          return resched | Sched_context::rq.current().deblock(sched(), current()->sched());
        }

      if (!_pending_rq.queued())
        {
          if (!q.first())
            ipi = true;

          q.enqueue(&_pending_rq);
        }
      else
        assert (_pending_rq.queue() == &q);
    }

  if (ipi)
    {
      //LOG_MSG_3VAL(this, "sipi", current_cpu(), cpu(), (Mword)current());
      Ipi::send(Ipi::Request, current_cpu(), target_cpu);
    }

  return false;
}

/**
 * Set the scheduling parameters and optionally change the home CPU of this
 * thread currently running on a different CPU than current_cpu().
 *
 * If not migrating to an offline CPU, enqueue an RQ for the CPU this thread is
 * currently running on. The handler Context::Pending_rqq::handle_requests()
 * will initiate the migration on this thread's current home CPU.
 *
 * \param cpu  Old (current) home CPU of this thread.
 * \retval false  No rescheduling required.
 * \retval true  Rescheduling required.
 */
PRIVATE inline
bool
Thread::migrate_xcpu(Cpu_number cpu)
{
  bool ipi = false;

    {
      Queue &q = Context::_pending_rqq.cpu(cpu);
      auto g = lock_guard(q.q_lock());

      // already migrated
      if (cpu != access_once(&_home_cpu))
        return false;

      // now we are sure that this thread stays on 'cpu' because
      // we have the rqq lock of 'cpu'
      if (!Cpu::online(cpu))
        {
          Migration_state inf = start_migration();
          if (inf.is_done())
            return inf.need_resched(); // all done, nothing to do

          Migration *m = inf.migration();
          Cpu_number target_cpu = access_once(&m->cpu);
          // Dequeue from ready queue with spin lock held!
          migrate_away(m, true);
          g.reset();
          return migrate_to(target_cpu);
        }

      if (!_pending_rq.queued())
        {
          if (!q.first())
            ipi = true;

          q.enqueue(&_pending_rq);
        }
    }

  if (ipi)
    Ipi::send(Ipi::Request, current_cpu(), cpu);

  return false;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "string_buffer.h"
#include "kdb_ke.h"
#include "terminate.h"

IMPLEMENT
Thread::Dbg_stack::Dbg_stack()
{
  stack_top = Kmem_alloc::allocator()->alloc(Bytes(Stack_size));
  if (stack_top)
    stack_top = static_cast<char *>(stack_top) + Stack_size;
  //printf("JDB STACK start= %p - %p\n", (char *)stack_top - Stack_size, (char *)stack_top);
}

DEFINE_PER_CPU Per_cpu<Thread::Dbg_stack> Thread::dbg_stack;

PUBLIC [[noreturn]] static
void
Thread::system_abort()
{
  if (nested_trap_handler)
    kdb_ke("abort");

  terminate(EXIT_FAILURE);
}

IMPLEMENT
void
Thread::Migration_log::print(String_buffer *buf) const
{
  buf->printf("migrate from %u to %u (state=%lx user ip=%lx)",
              cxx::int_value<Cpu_number>(src_cpu),
              cxx::int_value<Cpu_number>(target_cpu), state, user_ip);
}

PUBLIC
void
Thread::halt()
{
  // Cancel must be cleared on all kernel entry paths. See slowtraps for
  // why we delay doing it until here.
  state_del(Thread_cancel);

  // we haven't been re-initialized (cancel was not set) -- so sleep
  if (state_change_safely(~Thread_ready, Thread_cancel | Thread_dead))
    while (! (state() & Thread_ready))
      schedule();
}

PUBLIC static
void
Thread::halt_current()
{
  for (;;)
    {
      current_thread()->halt();
      kdb_ke("Thread not halted");
    }
}

//----------------------------------------------------------------------------
IMPLEMENTATION [irq_direct_inject]:

/**
 * Fake IRQ Chip class for doorbell interrupt notifications.
 * This chip uses the IRQ number as thread pointer and implements
 * the bind and unbind functionality.
 */
class Doorbell_irq_chip : public Irq_chip_soft
{
public:
  static Global_data<Doorbell_irq_chip> chip;
};

DEFINE_GLOBAL Global_data<Doorbell_irq_chip> Doorbell_irq_chip::chip;

PUBLIC static inline
Thread *Doorbell_irq_chip::thread(Mword pin)
{ return reinterpret_cast<Thread*>(pin); }

PUBLIC static inline
Mword Doorbell_irq_chip::pin(Thread *t)
{ return reinterpret_cast<Mword>(t); }

PUBLIC inline
void
Doorbell_irq_chip::detach(Irq_base *irq) override
{
  thread(irq->pin())->remove_doorbell_irq(irq);
  Irq_chip_soft::detach(irq);
}


PUBLIC
bool
Thread::register_doorbell_irq(Irq_base *irq)
{
  if (_doorbell_irq)
    return false;

  auto g = lock_guard(irq->irq_lock());
  irq->detach();
  Doorbell_irq_chip::chip->bind(irq, reinterpret_cast<Mword>(this));
  if (cas<Irq_base *>(&_doorbell_irq, nullptr, irq))
    return true;

  irq->detach();
  return false;
}

PUBLIC
void
Thread::remove_doorbell_irq(Irq_base *irq)
{
  cas<Irq_base *>(&_doorbell_irq, irq, nullptr);
}


PRIVATE inline
void
Thread::unregister_doorbell_irq()
{
  auto cpu_guard = lock_guard(cpu_lock);
  if (Irq_base *doorbell_irq = access_once(&_doorbell_irq))
    {
      auto irq_guard = lock_guard(doorbell_irq->irq_lock());
      doorbell_irq->detach();
    }
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!irq_direct_inject]:

PRIVATE inline
void
Thread::unregister_doorbell_irq()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

#include "terminate.h"

PUBLIC [[noreturn]] static
void
Thread::system_abort()
{ terminate(EXIT_FAILURE); }

//----------------------------------------------------------------------------
IMPLEMENTATION [!mbt_counter]:

PUBLIC inline void Thread::increment_mbt_counter() {}

//----------------------------------------------------------------------------
IMPLEMENTATION [mbt_counter]:

PUBLIC
inline void
Thread::increment_mbt_counter()
{
  // Model-Based Testing: increment testcounter if requested
  Utcb *utcb = this->utcb().access(true);
  if (utcb->user[2] == 0xdeadbeef)
    {
      atomic_add(&Kip::k()->mbt_counter, 1);
      utcb->user[2] = 0;
    }
}
