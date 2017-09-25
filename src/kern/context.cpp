INTERFACE:

#include <csetjmp>             // typedef jmp_buf
#include "types.h"
#include "clock.h"
#include "config.h"
#include "continuation.h"
#include "fpu_state.h"
#include "globals.h"
#include "l4_types.h"
#include "member_offs.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "queue.h"
#include "queue_item.h"
#include "rcupdate.h"
#include "sched_context.h"
#include "space.h"
#include "spin_lock.h"
#include "timeout.h"
#include <fiasco_defs.h>
#include <cxx/function>

class Entry_frame;
class Context;
class Kobject_iface;

class Context_ptr
{
public:
  explicit Context_ptr(Cap_index id) : _t(id) {}
  Context_ptr() {}
  Context_ptr(Context_ptr const &o) : _t(o._t) {}
  Context_ptr const &operator = (Context_ptr const &o)
  { _t = o._t; return *this; }

  Kobject_iface *ptr(Space *, L4_fpage::Rights *) const;

  bool is_kernel() const { return false; }
  bool is_valid() const { return _t != Cap_index(~0UL); }

  // only for debugging use
  Cap_index raw() const { return _t; }

private:
  Cap_index _t;
};

template< typename T >
class Context_ptr_base : public Context_ptr
{
public:
  enum Invalid_type { Invalid };
  enum Null_type { Null };
  explicit Context_ptr_base(Invalid_type) : Context_ptr(Cap_index(~0UL)) {}
  explicit Context_ptr_base(Null_type) : Context_ptr(Cap_index(0)) {}
  explicit Context_ptr_base(Cap_index id) : Context_ptr(id) {}
  Context_ptr_base() {}
  Context_ptr_base(Context_ptr_base<T> const &o) : Context_ptr(o) {}
  template< typename X >
  Context_ptr_base(Context_ptr_base<X> const &o) : Context_ptr(o)
  { X*x = 0; T*t = x; (void)t; }

  Context_ptr_base<T> const &operator = (Context_ptr_base<T> const &o)
  { Context_ptr::operator = (o); return *this; }

  template< typename X >
  Context_ptr_base<T> const &operator = (Context_ptr_base<X> const &o)
  { X*x=0; T*t=x; (void)t; Context_ptr::operator = (o); return *this; }
};

class Context_space_ref
{
public:
  typedef Spin_lock_coloc<Space *> Space_n_lock;

private:
  Space_n_lock _s;
  Address _v;

public:
  Space *space() const { return _s.get_unused(); }
  Space_n_lock *lock() { return &_s; }
  Address user_mode() const { return _v & 1; }
  Space *vcpu_user() const { return reinterpret_cast<Space*>(_v & ~3); }
  Space *vcpu_aware() const { return user_mode() ? vcpu_user() : space(); }

  void space(Space *s) { _s.set_unused(s); }
  void vcpu_user(Space *s) { _v = (Address)s; }
  void user_mode(bool enable)
  {
    if (enable)
      _v |= (Address)1;
    else
      _v &= (Address)(~1);
  }
};

/** An execution context.  A context is a runnable, schedulable activity.
    It carries along some state used by other subsystems: A lock count,
    and stack-element forward/next pointers.
 */
class Context :
  public Context_base,
  protected Rcu_item
{
  MEMBER_OFFSET();
  friend class Jdb_thread_list;
  friend class Context_ptr;
  friend class Jdb_utcb;

  struct State_request
  {
    Spin_lock<> lock;
    Mword add;
    Mword del;

    bool pending() const { return access_once(&add) || access_once(&del); }
  };

  State_request _remote_state_change;

protected:
  virtual void finish_migration() = 0;
  virtual bool initiate_migration() = 0;


public:
  /**
   * \brief Encapsulate an aggregate of Context.
   *
   * Allow to get a back reference to the aggregating Context object.
   */
  class Context_member
  {
  private:
    Context_member(Context_member const &);

  public:
    Context_member() {}
    /**
     * \brief Get the aggregating Context object.
     */
    Context *context() const;
  };

  /**
   * \brief Deferred Request.
   *
   * Represents a request that can be queued for each Context
   * and is executed by the target context just after switching to the
   * target context.
   */
  class Drq : public Queue_item, public Context_member
  {
  public:
    struct Result
    {
      unsigned char v;
      CXX_BITFIELD_MEMBER(0, 0, need_resched, v);
      CXX_BITFIELD_MEMBER(1, 1, no_answer, v);
    };

    static Result done()
    { Result r; r.v = 0; return r; }

    static Result no_answer()
    { Result r; r.v = Result::no_answer_bfm_t::Mask; return r; }

    static Result need_resched()
    { Result r; r.v = Result::need_resched_bfm_t::Mask; return r; }

    static Result no_answer_resched()
    {
      Result r;
      r.v = Result::no_answer_bfm_t::Mask | Result::need_resched_bfm_t::Mask;
      return r;
    }

    typedef Result (Request_func)(Drq *, Context *target, void *);
    enum Wait_mode { No_wait = 0, Wait = 1 };
    enum Exec_mode { Target_ctxt = 0, Any_ctxt = 1 };
    // enum State { Idle = 0, Handled = 1, Reply_handled = 2 };

    Request_func *func;
    void *arg;
    // State state;
  };

  /**
   * \brief Queue for deferred requests (Drq).
   *
   * A FIFO queue each Context aggregates to queue incoming Drq's
   * that have to be executed directly after switching to a context.
   */
  class Drq_q : public Queue, public Context_member
  {
  public:
    enum Drop_mode { Drop = true, No_drop = false };
    void enq(Drq *rq);
    bool dequeue(Drq *drq, Queue_item::Status reason);
    bool handle_requests(Drop_mode drop = No_drop);
    bool execute_request(Drq *r, Drop_mode drop, bool local);
  };

  struct Migration
  {
    Cpu_number cpu;
    L4_sched_param const *sp;
    bool in_progress;

    Migration() : in_progress(false) {}
  };

  template<typename T>
  class Ku_mem_ptr : public Context_member
  {
    MEMBER_OFFSET();

  private:
    typename User<T>::Ptr _u;
    T *_k;

  public:
    Ku_mem_ptr() : _u(0), _k(0) {}
    Ku_mem_ptr(typename User<T>::Ptr const &u, T *k) : _u(u), _k(k) {}

    void set(typename User<T>::Ptr const &u, T *k)
    { _u = u; _k = k; }

    T *access(bool is_current = false) const
    {
      // assert (!is_current || current() == context());
      if (is_current
          && (int)Config::Access_user_mem == Config::Access_user_mem_direct)
        return _u.get();

      Cpu_number const cpu = current_cpu();
      if ((int)Config::Access_user_mem == Config::Must_access_user_mem_direct
          && cpu == context()->home_cpu()
          && Mem_space::current_mem_space(cpu) == context()->space())
        return _u.get();
      return _k;
    }

    typename User<T>::Ptr usr() const { return _u; }
    T* kern() const { return _k; }
  };

public:
  /**
   * Definition of different helping modes
   */
  enum Helping_mode
  {
    Helping,
    Not_Helping,
    Ignore_Helping
  };

  enum class Switch
  {
    Ok      = 0,
    Resched = 1,
    Failed  = 2
  };

  /**
   * Return consumed CPU time.
   * @return Consumed CPU time in usecs
   */
  Cpu_time consumed_time();

  virtual bool kill() = 0;

  void spill_user_state();
  void fill_user_state();
  void copy_and_sanitize_trap_state(Trap_state *dst,
                                    Trap_state const *src) const;

  Space * FIASCO_PURE space() const { return _space.space(); }
  Mem_space * FIASCO_PURE mem_space() const { return static_cast<Mem_space*>(space()); }

  Cpu_number home_cpu() const { return _home_cpu; }

protected:
  Cpu_number _home_cpu;

  /**
   * Update consumed CPU time during each context switch and when
   *        reading out the current thread's consumed CPU time.
   */
  void update_consumed_time();

  Mword *_kernel_sp;
  void *_utcb_handler;
  Ku_mem_ptr<Utcb> _utcb;

private:
  friend class Jdb;
  friend class Jdb_tcb;

  /// low level page table switching
  void switchin_context(Context *) asm ("switchin_context_label") FIASCO_FASTCALL;

  /// low level fpu switching
  void switch_fpu(Context *t);

  /// low level cpu switching
  void switch_cpu(Context *t);

protected:
  Context_space_ref _space;

private:
  Context *_donatee;
  Context *_helper;

  // Lock state
  // how many locks does this thread hold on other threads
  // incremented in Thread::lock, decremented in Thread::clear
  // Thread::kill needs to know
  int _lock_cnt;

  // The scheduling parameters.  We would only need to keep an
  // anonymous reference to them as we do not need them ourselves, but
  // we aggregate them for performance reasons.
  Sched_context _sched_context;
  Sched_context *_sched;

  // Pointer to floating point register state
  Fpu_state _fpu_state;
  // Implementation-specific consumed CPU time (TSC ticks or usecs)
  Clock::Time _consumed_time;

  Drq _drq;
  Drq_q _drq_q;

protected:
  // for trigger_exception
  Continuation _exc_cont;

  jmp_buf *_recover_jmpbuf;     // setjmp buffer for page-fault recovery

  Migration *_migration;

public:
  void arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load);

protected:
  void arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load);
  void arch_update_vcpu_state(Vcpu_state *vcpu);
  void arch_vcpu_ext_shutdown();

  // XXX Timeout for both, sender and receiver! In normal case we would have
  // to define own timeouts in Receiver and Sender but because only one
  // timeout can be set at one time we use the same timeout. The timeout
  // has to be defined here because Dirq::hit has to be able to reset the
  // timeout (Irq::_irq_thread is of type Receiver).
  Timeout *_timeout;

  struct Kernel_drq : Drq { Context *src; };

private:
  static Per_cpu<Clock> _clock;
  static Per_cpu<Context *> _kernel_ctxt;
  static Per_cpu<Kernel_drq> _kernel_drq;
};


INTERFACE [debug]:

#include "tb_entry.h"

EXTENSION class Context
{
public:
  struct Drq_log : public Tb_entry
  {
    void *func;
    Context *thread;
    Drq const *rq;
    Cpu_number target_cpu;
    enum class Type
    {
      Send, Do_send, Do_request, Send_reply, Do_reply, Done
    } type;
    bool wait;
    void print(String_buffer *buf) const;
    Group_order has_partner() const
    {
      switch (type)
        {
        case Type::Send: return Group_order::first();
        case Type::Do_send: return Group_order(1);
        case Type::Do_request: return Group_order(2);
        case Type::Send_reply: return Group_order(3);
        case Type::Do_reply: return Group_order(4);
        case Type::Done: return Group_order::last();
        }
      return Group_order::none();
    }

    Group_order is_partner(Drq_log const *o) const
    {
      if (rq != o->rq || func != o->func)
        return Group_order::none();

      return o->has_partner();
    }
  };


  struct Vcpu_log : public Tb_entry
  {
    Mword state;
    Mword ip;
    Mword sp;
    Mword space;
    Mword err;
    unsigned char type;
    unsigned char trap;
    void print(String_buffer *buf) const;
  };
};

// --------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>
#include "atomic.h"
#include "cpu.h"
#include "cpu_lock.h"
#include "entry_frame.h"
#include "fpu.h"
#include "globals.h"		// current()
#include "lock_guard.h"
#include "logdefs.h"
#include "mem.h"
#include "mem_layout.h"
#include "processor.h"
#include "space.h"
#include "std_macros.h"
#include "thread_state.h"
#include "timer.h"
#include "timeout.h"

DEFINE_PER_CPU Per_cpu<Clock> Context::_clock(Per_cpu_data::Cpu_num);
DEFINE_PER_CPU Per_cpu<Context *> Context::_kernel_ctxt;
DEFINE_PER_CPU Per_cpu<Context::Kernel_drq> Context::_kernel_drq;

IMPLEMENT inline NEEDS["assert.h"]
Kobject_iface * __attribute__((nonnull(2, 3)))
Context_ptr::ptr(Space *s, L4_fpage::Rights *rights) const
{
  assert (cpu_lock.test());

  return static_cast<Obj_space*>(s)->lookup_local(_t, rights);
}



#include <cstdio>

/** Initialize a context.  After setup, a switch_exec to this context results
    in a return to user code using the return registers at regs().  The
    return registers are not initialized however; neither is the space_context
    to be used in thread switching (use set_space_context() for that).
    @pre (_kernel_sp == 0)  &&  (* (stack end) == 0)
 */
PUBLIC inline NEEDS ["atomic.h", "entry_frame.h", <cstdio>]
Context::Context()
: _kernel_sp(reinterpret_cast<Mword*>(regs())),
  /* TCBs are zero initialized _utcb_handler(0), */
  _helper(this),
  _sched_context(),
  _sched(&_sched_context)
  /* TCBs should be initialized to zero!
  _migration(0),
  _need_to_finish_migration(false)
  */
{
  // NOTE: We do not have to synchronize the initialization of
  // _space_context because it is constant for all concurrent
  // invocations of this constructor.  When two threads concurrently
  // try to create a new task, they already synchronize in
  // sys_task_new() and avoid calling us twice with different
  // space_context arguments.

  _home_cpu = Cpu::invalid();
}

PUBLIC inline
void
Context::reset_kernel_sp()
{
  _kernel_sp = reinterpret_cast<Mword*>(regs());
}

PUBLIC inline
void
Context::spill_fpu_if_owner()
{
  // spill FPU state into memory before migration
  if (state() & Thread_fpu_owner)
    {
      Fpu &f = Fpu::fpu.current();
      if (current() != this)
        f.enable();

      spill_fpu();
      f.set_owner(0);
      f.disable();
    }
}

PUBLIC static
void
Context::spill_current_fpu(Cpu_number cpu)
{
  (void)cpu;
  assert (cpu == current_cpu());
  Fpu &f = Fpu::fpu.current();
  if (f.owner())
    {
      f.enable();
      f.owner()->spill_fpu();
      f.set_owner(0);
      f.disable();
    }
}


PUBLIC inline
void
Context::release_fpu_if_owner()
{
  // If this context owned the FPU, noone owns it now
  Fpu &f = Fpu::fpu.current();
  if (f.is_owner(this))
    {
      f.set_owner(0);
      f.disable();
    }
}

/** Destroy context.
 */
PUBLIC virtual
Context::~Context()
{}

PUBLIC inline
bool
Context::check_for_current_cpu() const
{
  Cpu_number hc = access_once(&_home_cpu);
  bool r = hc == current_cpu() || !Cpu::online(hc);
  if (0 && EXPECT_FALSE(!r)) // debug output disabled
    printf("FAIL: cpu=%u (current=%u) %p current=%p\n",
           cxx::int_value<Cpu_number>(hc),
           cxx::int_value<Cpu_number>(current_cpu()), this, current());
  return r;
}


PUBLIC inline
Mword
Context::state(bool check = false) const
{
  (void)check;
  assert(!check || check_for_current_cpu());
  return _state;
}

PUBLIC static inline
Context*
Context::kernel_context(Cpu_number cpu)
{ return _kernel_ctxt.cpu(cpu); }

PROTECTED static inline
void
Context::kernel_context(Cpu_number cpu, Context *ctxt)
{ _kernel_ctxt.cpu(cpu) = ctxt; }


/** @name State manipulation */
//@{
//-


/**
 * Does the context exist? .
 * @return true if this context has been initialized.
 */
PUBLIC inline NEEDS [Context::is_invalid]
Mword
Context::exists() const
{
  return !is_invalid();
}

/**
 * Is the context about to be deleted.
 * @return true if this context is in deletion.
 */
PUBLIC inline NEEDS ["thread_state.h"]
bool
Context::is_invalid(bool check_cpu_local = false) const
{
  unsigned s = state(check_cpu_local);
  return (s & Thread_dead);
}

/**
 * Atomically add bits to state flags.
 * @param bits bits to be added to state flags
 * @return 1 if none of the bits that were added had been set before
 */
PUBLIC inline NEEDS ["atomic.h"]
void
Context::state_add(Mword bits)
{
  assert(check_for_current_cpu());
  atomic_or(&_state, bits);
}

/**
 * Add bits in state flags. Unsafe (non-atomic) and
 *        fast version -- you must hold the kernel lock when you use it.
 * @pre cpu_lock.test() == true
 * @param bits bits to be added to state flags
 */
PUBLIC inline
void
Context::state_add_dirty(Mword bits, bool check = true)
{
  (void)check;
  assert(!check || check_for_current_cpu());
  _state |= bits;
}

/**
 * Atomically delete bits from state flags.
 * @param bits bits to be removed from state flags
 * @return 1 if all of the bits that were removed had previously been set
 */
PUBLIC inline NEEDS ["atomic.h"]
void
Context::state_del(Mword bits)
{
  assert (check_for_current_cpu());
  atomic_and(&_state, ~bits);
}

/**
 * Delete bits in state flags. Unsafe (non-atomic) and
 *        fast version -- you must hold the kernel lock when you use it.
 * @pre cpu_lock.test() == true
 * @param bits bits to be removed from state flags
 */
PUBLIC inline
void
Context::state_del_dirty(Mword bits, bool check = true)
{
  (void)check;
  assert(!check || check_for_current_cpu());
  _state &= ~bits;
}

/**
 * Atomically delete and add bits in state flags, provided the
 *        following rules apply (otherwise state is not changed at all):
 *        - Bits that are to be set must be clear in state or clear in mask
 *        - Bits that are to be cleared must be set in state
 * @param mask Bits not set in mask shall be deleted from state flags
 * @param bits Bits to be added to state flags
 * @return 1 if state was changed, 0 otherwise
 */
PUBLIC inline NEEDS ["atomic.h"]
Mword
Context::state_change_safely(Mword mask, Mword bits)
{
  assert (check_for_current_cpu());
  Mword old;

  do
    {
      old = _state;
      if ((old & bits & mask) | (~old & ~mask))
        return 0;
    }
  while (!cas(&_state, old, (old & mask) | bits));

  return 1;
}

/**
 * Atomically delete and add bits in state flags.
 * @param mask bits not set in mask shall be deleted from state flags
 * @param bits bits to be added to state flags
 */
PUBLIC inline NEEDS ["atomic.h"]
Mword
Context::state_change(Mword mask, Mword bits)
{
  assert (check_for_current_cpu());
  return atomic_change(&_state, mask, bits);
}

/**
 * Delete and add bits in state flags. Unsafe (non-atomic) and
 *        fast version -- you must hold the kernel lock when you use it.
 * @pre cpu_lock.test() == true
 * @param mask Bits not set in mask shall be deleted from state flags
 * @param bits Bits to be added to state flags
 */
PUBLIC inline
void
Context::state_change_dirty(Mword mask, Mword bits, bool check = true)
{
  (void)check;
  assert(!check || check_for_current_cpu());
  _state &= mask;
  _state |= bits;
}

//@}
//-


PUBLIC inline
Context_space_ref *
Context::space_ref()
{ return &_space; }

PUBLIC inline
Space *
Context::vcpu_aware_space() const
{ return _space.vcpu_aware(); }

/** Registers used when iret'ing to user mode.
    @return return registers
 */
PUBLIC inline NEEDS["cpu.h", "entry_frame.h"]
Entry_frame *
Context::regs() const
{
  return reinterpret_cast<Entry_frame *>
    (Cpu::stack_align(reinterpret_cast<Mword>(this) + Size)) - 1;
}

/** Lock count.
    @return lock count
 */
PUBLIC inline
int
Context::lock_cnt() const
{
  return _lock_cnt;
}

/**
 * Switch active timeslice of this Context.
 * @param next Sched_context to switch to
 */
PUBLIC
void
Context::switch_sched(Sched_context *next, Sched_context::Ready_queue *queue)
{
  queue->switch_sched(sched(), next);
  set_sched(next);
}

/**
 * Select a different context for running and activate it.
 */
PUBLIC
void
Context::schedule()
{
  auto guard = lock_guard(cpu_lock);
  assert (!Sched_context::rq.current().schedule_in_progress);

  // we give up the CPU as a helpee, so we have no helper anymore
  if (EXPECT_FALSE(helper() != 0))
    set_helper(Not_Helping);

  // if we are a thread on a foreign CPU we must ask the kernel context to
  // schedule for us
  Cpu_number current_cpu = ::current_cpu();
  while (EXPECT_FALSE(current_cpu != access_once(&_home_cpu)))
    {
      Context *kc = Context::kernel_context(current_cpu);
      assert (this != kc);

      // flag that we need to schedule
      kc->state_add_dirty(Thread_need_resched);
      switch (switch_exec_locked(kc, Ignore_Helping))
        {
        case Switch::Ok:
          return;
        case Switch::Resched:
          current_cpu = ::current_cpu();
          continue;
        case Switch::Failed:
          assert (false);
          continue;
        }
    }

  // now, we are sure that a thread on its home CPU calls schedule.
  CNT_SCHEDULE;

  // Ensure only the current thread calls schedule
  assert (this == current());

  Sched_context::Ready_queue *rq = &Sched_context::rq.current();

  // Enqueue current thread into ready-list to schedule correctly
  update_ready_list();

  // Select a thread for scheduling.
  Context *next_to_run;

  for (;;)
    {
      next_to_run = rq->next_to_run()->context();

      // Ensure ready-list sanity
      assert (next_to_run);

      if (EXPECT_FALSE(!(next_to_run->state() & Thread_ready_mask)))
        rq->ready_dequeue(next_to_run->sched());
      else switch (schedule_switch_to_locked(next_to_run))
        {
        default:
        case Switch::Ok:      return;   // ok worked well
        case Switch::Failed:  break;    // not migrated, need preemption point
        case Switch::Resched:
          {
            Cpu_number n = ::current_cpu();
            if (n != current_cpu)
              {
                current_cpu = n;
                rq = &Sched_context::rq.current();
              }
          }
          continue; // may have been migrated...
        }

      rq->schedule_in_progress = this;
      Proc::preemption_point();
      if (EXPECT_TRUE(current_cpu == ::current_cpu()))
        rq->schedule_in_progress = 0;
      else
        return; // we got migrated and selected on our new CPU, so wen may run
    }
}


PUBLIC inline
void
Context::schedule_if(bool s)
{
  if (!s || Sched_context::rq.current().schedule_in_progress)
    return;

  schedule();
}

/**
 * Return Context's Sched_context with id 'id'; return time slice 0 as default.
 * @return Sched_context with id 'id' or 0
 */
PUBLIC inline
Sched_context *
Context::sched_context(unsigned short const id = 0) const
{
  if (EXPECT_TRUE (!id))
    return const_cast<Sched_context*>(&_sched_context);
  return 0;
}

/**
 * Return Context's currently active Sched_context.
 * @return Active Sched_context
 */
PUBLIC inline
Sched_context *
Context::sched() const
{
  return _sched;
}

/**
 * Set Context's currently active Sched_context.
 * @param sched Sched_context to be activated
 */
PROTECTED inline
void
Context::set_sched(Sched_context * const sched)
{
  _sched = sched;
}

// queue operations

// XXX for now, synchronize with global kernel lock
//-

/**
 * Enqueue current() if ready to fix up ready-list invariant.
 */
PRIVATE inline
void
Context::update_ready_list()
{
  assert (this == current());

  if ((state() & Thread_ready_mask) && sched()->left())
    Sched_context::rq.current().ready_enqueue(sched());
}

/**
 * Check if Context is in ready-list.
 * @return 1 if thread is in ready-list, 0 otherwise
 */
PUBLIC inline
Mword
Context::in_ready_list() const
{
  return sched()->in_ready_list();
}


/**
 * \brief Activate a newly created thread.
 *
 * This function sets a new thread onto the ready list and switches to
 * the thread if it can preempt the currently running thread.
 */
PUBLIC
void
Context::activate()
{
  auto guard = lock_guard(cpu_lock);
  if (xcpu_state_change(~0UL, Thread_ready, true))
    current()->switch_to_locked(this);
}


/** Helper.  Context that helps us by donating its time to us. It is
    set by switch_exec() if the calling thread says so.
    @return context that helps us and should be activated after freeing a lock.
*/
PUBLIC inline
Context *
Context::helper() const
{
  return _helper;
}


PUBLIC inline
void
Context::set_helper(Helping_mode const mode)
{
  switch (mode)
    {
    case Helping:
      _helper = current();
      break;
    case Not_Helping:
      _helper = this;
      break;
    case Ignore_Helping:
      // don't change _helper value
      break;
    }
}

/** Donatee.  Context that receives our time slices, for example
    because it has locked us.
    @return context that should be activated instead of us when we're
            switch_exec()'ed.
*/
PUBLIC inline
Context *
Context::donatee() const
{
  return _donatee;
}

PUBLIC inline
void
Context::set_donatee(Context * const donatee)
{
  _donatee = donatee;
}

PUBLIC inline
void
Context::set_kernel_sp(Mword * const esp)
{
  _kernel_sp = esp;
}

PUBLIC inline
Fpu_state *
Context::fpu_state()
{
  return &_fpu_state;
}

/**
 * Add to consumed CPU time.
 * @param quantum Implementation-specific time quantum (TSC ticks or usecs)
 */
PUBLIC inline
void
Context::consume_time(Clock::Time quantum)
{
  _consumed_time += quantum;
}

/**
 * Update consumed CPU time during each context switch and when
 *        reading out the current thread's consumed CPU time.
 */
IMPLEMENT inline NEEDS ["cpu.h"]
void
Context::update_consumed_time()
{
  if (Config::Fine_grained_cputime)
    consume_time(_clock.current().delta());
}

IMPLEMENT inline NEEDS ["config.h", "cpu.h"]
Cpu_time
Context::consumed_time()
{
  if (Config::Fine_grained_cputime)
    return _clock.current().us(_consumed_time);

  return _consumed_time;
}


/**
 * Switch scheduling context and execution context.
 * @param t Destination thread whose scheduling context and execution context
 *          should be activated.
 */
PROTECTED inline NEEDS ["assert.h", Context::switch_handle_drq]
Context::Switch FIASCO_WARN_RESULT
Context::schedule_switch_to_locked(Context *t)
{
   // Must be called with CPU lock held
  assert (cpu_lock.test());

  Sched_context::Ready_queue &rq = Sched_context::rq.current();
  // Switch to destination thread's scheduling context
  if (rq.current_sched() != t->sched())
    rq.set_current_sched(t->sched());

  if (EXPECT_FALSE(t == this))
    return switch_handle_drq();

  return switch_exec_locked(t, Not_Helping);
}

PUBLIC inline NEEDS [Context::schedule_switch_to_locked]
void
Context::switch_to_locked(Context *t)
{
  if (EXPECT_FALSE(schedule_switch_to_locked(t) != Switch::Ok))
    schedule();
}

PUBLIC inline NEEDS [Context::switch_to_locked]
bool
Context::deblock_and_schedule(Context *to)
{
  if (Sched_context::rq.current().deblock(to->sched(), sched(), true))
    {
      switch_to_locked(to);
      return true;
    }

  return false;
}


/**
 * Switch execution context while not running under CPU lock.
 */

PRIVATE inline
Context *
Context::handle_helping(Context *t)
{
  // XXX: maybe we do not need this on MP, because we have no helping there
  assert (current() == this);
  // Time-slice lending: if t is locked, switch to its locker
  // instead, this is transitive
  while (t->donatee() &&		// target thread locked
         t->donatee() != t)		// not by itself
    {
      // Special case for Thread::kill(): If the locker is
      // current(), switch to the locked thread to allow it to
      // release other locks.  Do this only when the target thread
      // actually owns locks.
      if (t->donatee() == this)
        {
          if (t->lock_cnt() > 0)
            break;

          return this;
        }

      t = t->donatee();
    }
  return t;
}


/**
 * Switch to a specific different execution context.
 *        If that context is currently locked, switch to its locker instead
 *        (except if current() is the locker)
 * @pre current() == this  &&  current() != t
 * @param t thread that shall be activated.
 * @param mode helping mode; we either help, don't help or leave the
 *             helping state unchanged
 */
PUBLIC
Context::Switch FIASCO_WARN_RESULT //L4_IPC_CODE
Context::switch_exec_locked(Context *t, enum Helping_mode mode)
{
  // Must be called with CPU lock held
  assert (t);
  assert (cpu_lock.test());
  assert (current() != t);
  assert (current() == this);

  // only for logging
  Context *t_orig = t;
  (void)t_orig;

  // Time-slice lending: if t is locked, switch to its locker
  // instead, this is transitive
  //
  // For the Thread_lock case only, so skip this
  // t = handle_helping(t);
  Context *ret = handle_helping(t);
  assert(ret == t);
  (void)ret;

  if (EXPECT_FALSE(t->running_on_different_cpu()))
    {
      if (!t->in_ready_list())
        Sched_context::rq.current().ready_enqueue(t->sched());
      return Switch::Failed;
    }


  LOG_CONTEXT_SWITCH;
  CNT_CONTEXT_SWITCH;

  // Can only switch to ready threads!
  // do not consider CPU locality here t can be temporarily migrated
  if (EXPECT_FALSE (!(t->state(false) & Thread_ready_mask)))
    {
      assert (state(false) & Thread_ready_mask);
      return Switch::Failed;
    }


  // Ensure kernel stack pointer is non-null if thread is ready
  assert (t->_kernel_sp);

  t->set_helper(mode);

  if (EXPECT_TRUE(current_cpu() == home_cpu()))
    update_ready_list();

  t->set_current_cpu(get_current_cpu());
  switch_fpu(t);
  switch_cpu(t);

  return switch_handle_drq();
}

PUBLIC
Context::Switch
Context::switch_exec_helping(Context *t, Mword const *lock, Mword val)
{
  // Must be called with CPU lock held
  assert (t);
  assert (cpu_lock.test());
  assert (current() != t);
  assert (current() == this);

  // only for logging
  Context *t_orig = t;
  (void)t_orig;

  // Time-slice lending: if t is locked, switch to its locker
  // instead, this is transitive
  //
  // For the Thread_lock case only, so skip this
  // t = handle_helping(t);

  // we actually hold locks
  if (!t->need_help(lock, val))
    return Switch::Failed;

  LOG_CONTEXT_SWITCH;

  // Can only switch to ready threads!
  // do not consider CPU locality here t can be temporarily migrated
  if (EXPECT_FALSE (!(t->state(false) & Thread_ready_mask)))
    {
      assert (state(false) & Thread_ready_mask);
      return Switch::Failed;
    }


  // Ensure kernel stack pointer is non-null if thread is ready
  assert (t->_kernel_sp);

  if (EXPECT_TRUE(get_current_cpu() == home_cpu()))
    update_ready_list();

  t->set_helper(Helping);
  t->set_current_cpu(get_current_cpu());
  switch_fpu(t);
  switch_cpu(t);
  return switch_handle_drq();
}

PUBLIC inline
Context::Ku_mem_ptr<Utcb> const &
Context::utcb() const
{ return _utcb; }

IMPLEMENT inline NEEDS["globals.h"]
Context *
Context::Context_member::context() const
{ return context_of(this); }

IMPLEMENT inline NEEDS["lock_guard.h", "assert.h"]
void
Context::Drq_q::enq(Drq *rq)
{
  assert(cpu_lock.test());
  auto guard = lock_guard(q_lock());
  enqueue(rq);
}

IMPLEMENT inline
bool
Context::Drq_q::execute_request(Drq *r, Drop_mode drop, bool local)
{
  bool need_resched = false;
  Context *const self = context();
  if (0)
    printf("CPU[%2u:%p]: context=%p: handle request for %p (func=%p, arg=%p)\n", cxx::int_value<Cpu_number>(current_cpu()), current(), context(), r->context(), r->func, r->arg);
  if (r->context() == self)
    {
      LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
          l->type = Drq_log::Type::Do_reply;
          l->rq = r;
          l->func = (void*)r->func;
          l->thread = r->context();
          l->target_cpu = current_cpu();
          l->wait = 0;
      );
      //LOG_MSG_3VAL(current(), "hrP", current_cpu() | (drop ? 0x100: 0), (Mword)r->context(), (Mword)r->func);
      self->state_change_dirty(~Thread_drq_wait, Thread_ready);
      self->handle_remote_state_change();
      return !(self->state() & Thread_ready_mask);
    }
  else
    {
      LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
          l->type = Drq_log::Type::Do_request;
          l->rq = r;
          l->func = (void*)r->func;
          l->thread = r->context();
          l->target_cpu = current_cpu();
          l->wait = 0;
      );

      Drq::Result answer = Drq::done();
      if (EXPECT_TRUE(drop == No_drop && r->func))
        {
          self->handle_remote_state_change();
          answer = r->func(r, self, r->arg);
        }
      else if (EXPECT_FALSE(drop == Drop))
        // flag DRQ abort for requester
        r->arg = (void*)-1;

      need_resched |= answer.need_resched();

      // enqueue answer
      if (!(answer.no_answer()))
        {
          Context *c = r->context();
          if (local)
            {
              c->state_change_dirty(~Thread_drq_wait, Thread_ready);
              return need_resched || !(c->state() & Thread_ready_mask);
            }
          else
            need_resched |= c->enqueue_drq(r, Drq::Target_ctxt);
        }
    }
  return need_resched;
}

IMPLEMENT inline NEEDS["lock_guard.h"]
bool
Context::Drq_q::dequeue(Drq *drq, Queue_item::Status reason)
{
  auto guard = lock_guard(q_lock());
  if (!drq->queued())
    return false;
  return Queue::dequeue(drq, reason);
}

IMPLEMENT inline NEEDS["mem.h", "lock_guard.h"]
bool
Context::Drq_q::handle_requests(Drop_mode drop)
{
  if (0)
    printf("CPU[%2u:%p]: > Context::Drq_q::handle_requests() context=%p\n", cxx::int_value<Cpu_number>(current_cpu()), current(), context());
  bool need_resched = false;
  while (1)
    {
      Queue_item *qi;
        {
          auto guard = lock_guard(q_lock());
          qi = first();
          if (!qi)
            return need_resched;

          check (Queue::dequeue(qi, Queue_item::Ok));
        }

      Drq *r = static_cast<Drq*>(qi);
      if (0)
        printf("CPU[%2u:%p]: context=%p: handle request for %p (func=%p, arg=%p)\n", cxx::int_value<Cpu_number>(current_cpu()), current(), context(), r->context(), r->func, r->arg);
      need_resched |= execute_request(r, drop, false);
    }
}

/**
 * \brief Forced dequeue from lock wait queue, or DRQ queue.
 */
PRIVATE
void
Context::force_dequeue()
{
  Queue_item *const qi = queue_item();

  if (qi->queued())
    {
      // we're waiting for a lock or have a DRQ pending
      Queue *const q = qi->queue();
        {
          auto guard = lock_guard(q->q_lock());
          // check again, with the queue lock held.
          // NOTE: we may be already removed from the queue on another CPU
          if (qi->queued() && qi->queue())
            {
              // we must never be taken from one queue to another on a
              // different CPU
              assert(q == qi->queue());
              // pull myself out of the queue, mark reason as invalidation
              q->dequeue(qi, Queue_item::Invalid);
            }
        }
    }
}

/**
 * \brief Dequeue from lock and DRQ queues, abort pending DRQs
 */
PROTECTED
void
Context::shutdown_queues()
{
  force_dequeue();
  shutdown_drqs();
}


/**
 * \brief Check for pending DRQs.
 * \return true if there are DRQs pending, false if not.
 */
PUBLIC inline
bool
Context::drq_pending() const
{ return _drq_q.first(); }

PUBLIC inline
void
Context::try_finish_migration()
{
  if (state() & Thread_finish_migration)
    {
      state_del_dirty(Thread_finish_migration);
      finish_migration();
    }
}


/**
 * \brief Handle all pending DRQs.
 * \pre cpu_lock.test() (The CPU lock must be held).
 * \pre current() == this (only the currently running context is allowed to
 *      call this function).
 * \return true if re-scheduling is needed (ready queue has changed),
 *         false if not.
 */
PUBLIC //inline
bool
Context::handle_drq()
{

  assert (check_for_current_cpu());
  assert (cpu_lock.test());

  bool resched = false;
  Mword st = state();
  if (EXPECT_FALSE(st & Thread_switch_hazards))
    {
      state_del_dirty(Thread_switch_hazards);
      if (st & Thread_finish_migration)
        finish_migration();

      if (st & Thread_need_resched)
        resched = true;
    }

  if (EXPECT_TRUE(!drq_pending()))
    return resched;

  Mem::barrier();
  resched |= _drq_q.handle_requests();
  state_del_dirty(Thread_drq_ready);

  //LOG_MSG_3VAL(this, "xdrq", state(), ret, cpu_lock.test());

  /*
   * When the context is marked as dead (Thread_dead) then we must not execute
   * any usual context code, however DRQ handlers may run.
   */
  if (state() & Thread_dead)
    state_del_dirty(Thread_ready_mask);

  return resched || !(state() & Thread_ready_mask);
}

PRIVATE inline
Context::Switch
Context::switch_handle_drq()
{
  if (EXPECT_TRUE(home_cpu() == get_current_cpu()))
    return EXPECT_FALSE(handle_drq()) ? Switch::Resched : Switch::Ok;
  return Switch::Ok;
}

/**
 * \brief Get the queue item of the context.
 * \pre The context must currently not be in any queue.
 * \return The queue item of the context.
 *
 * The queue item can be used to enqueue the context to a Queue.
 * a context must be in at most one queue at a time.
 * To figure out the context corresponding to a queue item
 * context_of() can be used.
 */
PUBLIC inline
Queue_item *
Context::queue_item()
{
  return &_drq;
}

PROTECTED inline
void
Context::handle_remote_state_change()
{
  if (!_remote_state_change.pending())
    return;

  Mword add, del;
    {
      auto guard = lock_guard(_remote_state_change.lock);
      add = access_once(&_remote_state_change.add);
      del = access_once(&_remote_state_change.del);
      _remote_state_change.add = 0;
      _remote_state_change.del = 0;
    }

  state_change_dirty(~del, add);
}

PUBLIC inline
void
Context::set_home_cpu(Cpu_number cpu)
{
  auto guard = lock_guard(_remote_state_change.lock);

  if (_remote_state_change.pending())
    {
      Mword add = access_once(&_remote_state_change.add);
      Mword del = access_once(&_remote_state_change.del);
      _remote_state_change.add = 0;
      _remote_state_change.del = 0;
      state_change_dirty(~del, add);
    }

  write_now(&_home_cpu, cpu);
}


/**
 * \brief Queue a DRQ for changing the contexts state.
 * \param mask bit mask for the state (state &= mask).
 * \param add bits to add to the state (state |= add).
 * \note This function is a preemption point.
 *
 * This function must be used to change the state of contexts that are
 * potentially running on a different CPU.
 */
PUBLIC inline NEEDS[Context::pending_rqq_enqueue]
bool
Context::xcpu_state_change(Mword mask, Mword add, bool lazy_q = false)
{
  Cpu_number current_cpu = ::current_cpu();
  if (EXPECT_FALSE(access_once(&_home_cpu) != current_cpu))
    {
      auto guard = lock_guard(_remote_state_change.lock);
      if (EXPECT_TRUE(access_once(&_home_cpu) != current_cpu))
        {
          _remote_state_change.add = (_remote_state_change.add & mask) | add;
          _remote_state_change.del = (_remote_state_change.del & ~add)  | ~mask;
          guard.reset();
          pending_rqq_enqueue();
          return false;
        }
    }

  state_change_dirty(mask, add);
  if (add & Thread_ready_mask)
    return Sched_context::rq.current().deblock(sched(), current()->sched(), lazy_q);
  return false;
}


/**
 * \brief Initiate a DRQ for the context.
 * \pre \a src must be the currently running context.
 * \param src the source of the DRQ (the context who initiates the DRQ).
 * \param func the DRQ handler.
 * \param arg the argument for the DRQ handler.
 *
 * DRQs are requests that any context can queue to any other context. DRQs are
 * the basic mechanism to initiate actions on remote CPUs in an MP system,
 * however, are also allowed locally.
 * DRQ handlers of pending DRQs are executed by Context::handle_drq() in the
 * context of the target context. Context::handle_drq() is basically called
 * after switching to a context in Context::switch_exec_locked().
 *
 * This function enqueues a DRQ and blocks the current context for a reply DRQ.
 */
PUBLIC inline NEEDS[Context::enqueue_drq, "logdefs.h"]
void
Context::drq(Drq *drq, Drq::Request_func *func, void *arg,
             Drq::Exec_mode exec = Drq::Target_ctxt,
             Drq::Wait_mode wait = Drq::Wait)
{
  if (0)
    printf("CPU[%2u:%p]: > Context::drq(this=%p, func=%p, arg=%p)\n", cxx::int_value<Cpu_number>(current_cpu()), current(), this, func,arg);
  Context *cur = current();
  LOG_TRACE("DRQ handling", "drq", cur, Drq_log,
      l->type = Drq_log::Type::Send;
      l->rq = drq;
      l->func = (void*)func;
      l->thread = this;
      l->target_cpu = home_cpu();
      l->wait = wait;
  );
  //assert (current() == src);
  assert (!(wait == Drq::Wait && (cur->state() & Thread_drq_ready)) || cur->home_cpu() == home_cpu());
  assert (!((wait == Drq::Wait || drq == &_drq) && cur->state() & Thread_drq_wait));
  assert (!drq->queued());

  drq->func  = func;
  drq->arg   = arg;
  if (wait == Drq::Wait)
    cur->state_add(Thread_drq_wait);


  enqueue_drq(drq, exec);

  //LOG_MSG_3VAL(src, "<drq", src->state(), Mword(this), 0);
  while (wait == Drq::Wait && cur->state() & Thread_drq_wait)
    {
      cur->state_del(Thread_ready_mask);
      cur->schedule();
    }

  LOG_TRACE("DRQ handling", "drq", cur, Drq_log,
      l->type = Drq_log::Type::Done;
      l->rq = drq;
      l->func = (void*)func;
      l->thread = this;
      l->target_cpu = home_cpu();
      l->wait = wait;
  );
  //LOG_MSG_3VAL(src, "drq>", src->state(), Mword(this), 0);
}

PUBLIC
bool
Context::kernel_context_drq(Drq::Request_func *func, void *arg)
{
  if (EXPECT_TRUE(home_cpu() == get_current_cpu()))
    update_ready_list();

  Context *kc = kernel_context(current_cpu());
  if (current() == kc)
    return func(0, kc, arg).need_resched();

  Kernel_drq *mdrq = new (&_kernel_drq.current()) Kernel_drq;

  mdrq->src = this;
  mdrq->func  = func;
  mdrq->arg   = arg;
  kc->_drq_q.enq(mdrq);
  return schedule_switch_to_locked(kc) != Switch::Ok;
}

PUBLIC inline NEEDS[Context::drq]
void
Context::drq(Drq::Request_func *func, void *arg,
             Drq::Exec_mode exec = Drq::Target_ctxt,
             Drq::Wait_mode wait = Drq::Wait)
{ return drq(&current()->_drq, func, arg, exec, wait); }

PRIVATE static
bool
Context::rcu_unblock(Rcu_item *i)
{
  assert(cpu_lock.test());
  return static_cast<Context*>(i)->xcpu_state_change(~Thread_waiting, Thread_ready);
}

PUBLIC inline
void
Context::recover_jmp_buf(jmp_buf *b)
{ _recover_jmpbuf = b; }

PUBLIC static
void
Context::xcpu_tlb_flush(...)
{
  // This should always be optimized away
  assert(0);
}

IMPLEMENT_DEFAULT inline
void
Context::arch_load_vcpu_kern_state(Vcpu_state *, bool)
{}

IMPLEMENT_DEFAULT inline
void
Context::arch_load_vcpu_user_state(Vcpu_state *, bool)
{}

IMPLEMENT_DEFAULT inline
void
Context::arch_vcpu_ext_shutdown()
{}

IMPLEMENT_DEFAULT inline
void
Context::arch_update_vcpu_state(Vcpu_state *)
{}

IMPLEMENT_DEFAULT inline
void
Context::copy_and_sanitize_trap_state(Trap_state *dst,
                                      Trap_state const *src) const
{ dst->copy_and_sanitize(src); }

PUBLIC inline
bool Context::migration_pending() const { return _migration; }

//----------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

#include "assert.h"
#include "warn.h"

PRIVATE inline
void
Context::handle_lock_holder_preemption()
{}

/** Increment lock count.
    @post lock_cnt() > 0 */
PUBLIC inline
void
Context::inc_lock_cnt()
{
  ++_lock_cnt;
}

/** Decrement lock count.
    @pre lock_cnt() > 0
 */
PUBLIC inline
void
Context::dec_lock_cnt()
{
  --_lock_cnt;
}

PRIVATE inline
bool
Context::running_on_different_cpu()
{ return false; }

PRIVATE inline
bool
Context::need_help(Mword const *, Mword)
{ return true; }


PRIVATE inline
void
Context::pending_rqq_enqueue()
{
  if (!Cpu::online(home_cpu()))
    handle_remote_state_change();
}

PUBLIC
bool
Context::enqueue_drq(Drq *rq, Drq::Exec_mode /*exec*/)
{
  assert (cpu_lock.test());

  LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
      l->type = rq->context() == this
                                 ? Drq_log::Type::Send_reply
                                 : Drq_log::Type::Do_send;
      l->func = (void*)rq->func;
      l->thread = this;
      l->target_cpu = home_cpu();
      l->wait = 0;
      l->rq = rq;
  );

  bool do_sched = _drq_q.execute_request(rq, Drq_q::No_drop, true);
  if (   access_once(&_home_cpu) == current_cpu()
      && (state() & Thread_ready_mask) && !in_ready_list())
    {
      Sched_context::rq.current().ready_enqueue(sched());
      return true;
    }
  return do_sched;
}


PRIVATE inline NOEXPORT
void
Context::shutdown_drqs()
{ _drq_q.handle_requests(Drq_q::Drop); }


PUBLIC inline
void
Context::rcu_wait()
{
  // The UP case does not need to block for the next grace period, because
  // the CPU is always in a quiescent state when the interrupts where enabled
}

//----------------------------------------------------------------------------
INTERFACE [mp]:

#include "queue.h"
#include "queue_item.h"

EXTENSION class Context
{
private:
  friend class Switch_lock;

  /**
   * The running-under-lock state machine.
   *
   * This state machine handles state transitions for the
   * Context::_running_under_lock variable to ensure correct multi-
   * processor helping semantics.
   *
   * We use three states:
   * 1. `Not_running` --- The thread is not running on a foreign CPU. However,
   *    it might be running on its home CPU without holding any helping lock.
   * 2. `Trying` --- A thread on a foreign CPU has the intention to start
   *    helping but has yet to evaluate if this thread owns the desired lock.
   * 3. `Running` --- This thread is running on a CPU while usually holding
   *    at least one helping lock.
   */
  class Running_under_lock
  {
    // FIXME: could be a byte, but nee cas byte for that
    enum State : Mword
    {
      Not_running = 0,
      Trying      = 1,
      Running     = 2,
    };

    State _s;

  public:
    /**
     * Atomic transition from `Not_running` to `Trying`.
     *
     * \retval true, on success. This means no other thread is currently
     *         helping or trying to help. However, the thread might
     *         currently execute on its home CPU.
     * \retval false, another thread is currently helping or trying to help.
     *         This means the helper shall retry to grab the helping lock.
     *
     * Must be used by a potential helper thread before double checking
     * if this thread owns the desired lock. If yes, then
     * Running_under_lock::help() must be called before helping starts.
     * If no, Running_under_lock::reset() must be called.
     */
    bool try_to_help()
    {
      if (_s)
        return false; // either running or already trying

      return mp_cas(&_s, Not_running, Trying);
    }

    /**
     * Atomic transition from `Not_running` to `Running`.
     *
     * \pre Must be called by the current thread on its own
     *      `_running_under_lock` member only.
     *
     * \retval true, on success. This means no other thread is currently
     *         helping or trying to help.
     * \retval false, another thread is currently trying to help. This means
     *         the caller must wait before grabbing the first lock until the
     *         potential helper aborted its helping.
     */
    bool try_dispatch()
    {
      if (_s)
        return false;

      return mp_cas(&_s, Not_running, Running);
    }

    /**
     * Transition from Trying to Running.
     *
     * Before calling this method Running_under_lock::try_to_help() must
     * have returned success.
     */
    void help() { write_now(&_s, Running); }

    /**
     * Dirty transition to Not_running.
     *
     * This method is to be used to abort an unsuccessful attempt to help
     * or after clearing the last lock when running on the home CPU.
     */
    void reset() { write_now(&_s, Not_running); }

    /**
     * Safe transition from Running to Not_running.
     *
     * This method has to be used to safely mark a thread as not running
     * in the preemption code (after switching away from this thread).
     */
    void preempt()
    {
      if (_s == Running)
        write_now(&_s, Not_running);
    }

    /// Check the current running under lock state.
    operator bool () const { return _s; }
  };

  /**
   * Synchronization variable for multi-processor helping.
   *
   * This variable in combination with the _lock_cnt variable is used to
   * synchronize dispatching of this context during cross-processor helping.
   */
  Running_under_lock _running_under_lock;

protected:

  class Pending_rqq : public Queue
  {
  public:
    static void enq(Context *c);
    bool handle_requests(Context **);
  };

  class Pending_rq : public Queue_item, public Context_member
  {} _pending_rq;

protected:
  static Per_cpu<Pending_rqq> _pending_rqq;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "assert.h"
#include "cpu_call.h"
#include "globals.h"
#include "ipi.h"
#include "lock_guard.h"
#include "mem.h"

DEFINE_PER_CPU Per_cpu<Context::Pending_rqq> Context::_pending_rqq;

PRIVATE inline
void
Context::handle_lock_holder_preemption()
{
  assert (current() != this);
  _running_under_lock.preempt();
}

/** Increment lock count.
    @post lock_cnt() > 0 */
PUBLIC inline
void
Context::inc_lock_cnt()
{
  write_now(&_lock_cnt, _lock_cnt + 1);
}

/** Decrement lock count.
    @pre lock_cnt() > 0
 */
PUBLIC inline
void
Context::dec_lock_cnt()
{
  int ncnt = _lock_cnt - 1;
  write_now(&_lock_cnt, ncnt);
  if (EXPECT_TRUE(ncnt == 0 && home_cpu() == current_cpu()))
    {
      Mem::mp_wmb();
      _running_under_lock.reset();
    }
}

PRIVATE inline
bool
Context::running_on_different_cpu()
{
  if (   EXPECT_TRUE(access_once(&_lock_cnt) == 0)
      && EXPECT_TRUE(!access_once(&_running_under_lock)))
    return false;

  if (EXPECT_FALSE(!_running_under_lock.try_dispatch()))
    return true;

  if (EXPECT_FALSE(access_once(&_lock_cnt) == 0))
    _running_under_lock.reset();

  return false;
}

PRIVATE inline
bool
Context::need_help(Mword const *lock, Mword val)
{
  if (EXPECT_FALSE(!_running_under_lock.try_to_help()))
    return false;

  Mem::mp_mb();

  // double check if the lock is held by us
  if (EXPECT_TRUE(access_once(&_lock_cnt) != 0 && access_once(lock) == val))
    {
      _running_under_lock.help();
      return true;
    }

  _running_under_lock.reset();
  return false;
}

/**
 * \brief Enqueue the given \a c into its CPUs queue.
 * \param c the context to enqueue for DRQ handling.
 */
IMPLEMENT inline NEEDS["globals.h", "lock_guard.h"]
void
Context::Pending_rqq::enq(Context *c)
{
  // FIXME: is it safe to do the check without a locked queue, or may
  //        we loose DRQs then?

  //if (!c->_pending_rq.queued())
    {
      Queue &q = Context::_pending_rqq.cpu(c->home_cpu());
      auto guard = lock_guard(q.q_lock());
      if (c->_pending_rq.queued())
        return;
      q.enqueue(&c->_pending_rq);
    }
}


/**
 * \brief Wakeup all contexts with pending DRQs.
 *
 * This function wakes up all context from the pending queue.
 */
IMPLEMENT
bool
Context::Pending_rqq::handle_requests(Context **mq)
{
  //LOG_MSG_3VAL(current(), "phq", current_cpu(), 0, 0);
  if (0)
    printf("CPU[%2u:%p]: Context::Pending_rqq::handle_requests() this=%p\n", cxx::int_value<Cpu_number>(current_cpu()), current(), this);
  bool resched = false;
  Context *curr = current();
  while (1)
    {
      Context *c;
        {
          auto guard = lock_guard(q_lock());
          Queue_item *qi = first();
          if (!qi)
            return resched;

          check (dequeue(qi, Queue_item::Ok));
          c = static_cast<Context::Pending_rq *>(qi)->context();
        }

      assert (c->check_for_current_cpu());

      c->handle_remote_state_change();
      if (EXPECT_FALSE(c->_migration != 0))
        {
          // if the currently executing thread shall be migrated we must defer
          // this until we have handled the whole request queue, otherwise we
          // would miss the remaining requests or execute them on the wrong CPU.
          if (c != curr)
            {
              // we can directly migrate the thread...
              resched |= c->initiate_migration();

              // if migrated away skip the resched test below
              if (access_once(&c->_home_cpu) != current_cpu())
                continue;
            }
          else
            *mq = c;
        }
      else
        c->try_finish_migration();

      if (EXPECT_TRUE(c->drq_pending()))
        {
          if (EXPECT_TRUE(c != curr))
            c->state_add(Thread_drq_ready);
          else
            resched |= c->handle_drq();
        }

      if (EXPECT_TRUE(c != curr && (c->state() & Thread_ready_mask)))
        {
          Sched_context *cs = (curr->home_cpu() == curr->get_current_cpu())
                            ? curr->sched()
                            : 0;

          resched |= Sched_context::rq.current().deblock(c->sched(), cs);
        }
    }
}

PUBLIC static inline NEEDS["cpu_call.h"]
bool
Context::take_cpu_offline(Cpu_number cpu, bool drain_rqq = false)
{
  assert (cpu == current_cpu());

    {
      auto &q = _pending_rqq.current();
      auto guard = lock_guard(q.q_lock());

      if (q.first())
        {
          if (!drain_rqq)
            return false;

          Context *migration_q = 0;
          q.handle_requests(&migration_q);
          // assume we run from the idle thread, and the idle thread does
          // never migrate so `migration_q` must be 0
          assert (!migration_q);
        }

      Cpu::cpus.current().set_online(false);
    }
  Mem::mp_mb();

  Rcu::do_pending_work(cpu);
  Cpu_call::handle_global_requests();
  return true;
}

PUBLIC static inline
void
Context::take_cpu_online(Cpu_number cpu)
{
  Cpu::cpus.cpu(cpu).set_online(true);
}

PRIVATE
void
Context::pending_rqq_enqueue()
{
  bool ipi = false;
  // read cpu again we may've been migrated meanwhile
  Cpu_number cpu = access_once(&_home_cpu);
  Queue &q = Context::_pending_rqq.cpu(cpu);

    {
      auto guard = lock_guard(q.q_lock());

      // migrated between getting the lock and reading the CPU, so the
      // new CPU is responsible for executing our request
      if (access_once(&_home_cpu) != cpu)
        return;

      if (!Cpu::online(cpu))
        {
          handle_remote_state_change();
          return;
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
}

PRIVATE inline
bool
Context::_execute_drq(Drq *rq, bool offline_cpu = false)
{
  bool do_sched = _drq_q.execute_request(rq, Drq_q::No_drop, true);
  assert (offline_cpu || home_cpu() == current_cpu());
  if (!in_ready_list() && (state(false) & Thread_ready_mask))
    {
      if (EXPECT_FALSE(offline_cpu))
        Sched_context::rq.cpu(home_cpu()).ready_enqueue(sched());
      else
        Sched_context::rq.current().ready_enqueue(sched());
      return true;
    }

  return do_sched;
}

PRIVATE inline
bool
Context::_deq_exec_drq(Drq *rq, bool offline_cpu = false)
{
  if (!_drq_q.dequeue(rq, Queue_item::Ok))
    return false; // already handled

  if (!drq_pending() && EXPECT_FALSE(state(false) & Thread_drq_ready))
    state_del_dirty(Thread_drq_ready);

  return _execute_drq(rq, offline_cpu);
}

PUBLIC
bool
Context::enqueue_drq(Drq *rq, Drq::Exec_mode /*exec*/)
{
  assert (cpu_lock.test());

  Cpu_number cpu = access_once(&_home_cpu);
  Cpu_number current_cpu = ::current_cpu();

  LOG_TRACE("DRQ handling", "drq", current(), Drq_log,
      l->type = rq->context() == this
                                 ? Drq_log::Type::Send_reply
                                 : Drq_log::Type::Do_send;
      l->func = (void*)rq->func;
      l->thread = this;
      l->target_cpu = cpu;
      l->wait = 0;
      l->rq = rq;
  );

  if (EXPECT_FALSE(cpu == current_cpu))
    return _execute_drq(rq);

  _drq_q.enq(rq);

  // re-read the cpu number, we may have been migrated. We need to be sure to
  // signal the right CPU that there is work for us.
  cpu = access_once(&_home_cpu);

  // check if we migrated to the current_cpu, in this case we have to execute
  // the DRQ directly
  if (EXPECT_FALSE(cpu == current_cpu))
    return _deq_exec_drq(rq);

  bool ipi = false;

    {
      Queue &q = Context::_pending_rqq.cpu(cpu);
      auto guard = lock_guard(q.q_lock());

      // migrated between getting the lock and reading the CPU, so the
      // new CPU is responsible for executing our request
      if (access_once(&_home_cpu) != cpu)
        return false;

      if (EXPECT_FALSE(!Cpu::online(cpu)))
        return _deq_exec_drq(rq, true);

      if (!_pending_rq.queued())
        {
          if (!q.first())
            ipi = true;

          q.enqueue(&_pending_rq);
        }
    }

  if (ipi)
    Ipi::send(Ipi::Request, current_cpu, cpu);

  return false;
}


PRIVATE inline NOEXPORT
void
Context::shutdown_drqs()
{
  if (_pending_rq.queued())
    {
      auto guard = lock_guard(_pending_rq.queue()->q_lock());
      if (_pending_rq.queued())
        _pending_rq.queue()->dequeue(&_pending_rq, Queue_item::Ok);
    }

  _drq_q.handle_requests(Drq_q::Drop);
}


/**
 * Block and wait for the next grace period.
 */
PUBLIC inline NEEDS["cpu_lock.h", "lock_guard.h"]
void
Context::rcu_wait()
{
  auto guard = lock_guard(cpu_lock);
  state_change_dirty(~Thread_ready, Thread_waiting);
  Rcu::call(this, &rcu_unblock);
  while (state() & Thread_waiting)
    {
      state_del_dirty(Thread_ready);
      schedule();
    }
}



//----------------------------------------------------------------------------
IMPLEMENTATION [fpu && !ux]:

#include "fpu.h"

PUBLIC inline NEEDS ["fpu.h"]
void
Context::spill_fpu()
{
  // If we own the FPU, we should never be getting an "FPU unavailable" trap
  assert (Fpu::fpu.current().owner() == this);
  assert (state() & Thread_fpu_owner);
  assert (fpu_state());

  // Save the FPU state of the previous FPU owner (lazy) if applicable
  Fpu::save_state(fpu_state());
  state_del_dirty(Thread_fpu_owner);
}


/**
 * When switching away from the FPU owner, disable the FPU to cause
 * the next FPU access to trap.
 * When switching back to the FPU owner, enable the FPU so we don't
 * get an FPU trap on FPU access.
 */
IMPLEMENT inline NEEDS ["fpu.h"]
void
Context::switch_fpu(Context *t)
{
  Fpu &f = Fpu::fpu.current();
  if (f.is_owner(this))
    f.disable();
  else if (f.is_owner(t) && !(t->state() & Thread_vcpu_fpu_disabled))
    f.enable();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!fpu]:

PUBLIC inline
void
Context::spill_fpu()
{}

IMPLEMENT inline
void
Context::switch_fpu(Context *)
{}

//----------------------------------------------------------------------------
INTERFACE [debug]:

#include "tb_entry.h"

/** logged context switch. */
class Tb_entry_ctx_sw : public Tb_entry
{
public:
  using Tb_entry::_ip;

  Context const *dst;		///< switcher target
  Context const *dst_orig;
  Address kernel_ip;
  Mword lock_cnt;
  Space const *from_space;
  Sched_context const *from_sched;
  Mword from_prio;
  void print(String_buffer *buf) const;
};



// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "kobject_dbg.h"
#include "string_buffer.h"

PUBLIC inline
Mword *
Context::get_kernel_sp() const
{
  return _kernel_sp;
}

IMPLEMENT
void
Context::Drq_log::print(String_buffer *buf) const
{
  static char const *const _types[] =
    { "send", "do send", "do request", "send reply", "do reply", "done" };

  char const *t = "unk";
  if ((unsigned)type < sizeof(_types)/sizeof(_types[0]))
    t = _types[(unsigned)type];

  buf->printf("%s(%s) rq=%p to ctxt=%lx/%p (func=%p) cpu=%u",
      t, wait ? "wait" : "no-wait", rq, Kobject_dbg::pointer_to_id(thread),
      thread, func, cxx::int_value<Cpu_number>(target_cpu));
}

// context switch
IMPLEMENT
void
Tb_entry_ctx_sw::print(String_buffer *buf) const
{
  Context *sctx = 0;
  Mword sctxid = ~0UL;
  Mword dst;
  Mword dst_orig;

  sctx = from_sched->context();
  sctxid = Kobject_dbg::pointer_to_id(sctx);

  dst = Kobject_dbg::pointer_to_id(this->dst);
  dst_orig = Kobject_dbg::pointer_to_id(this->dst_orig);

  if (sctx != ctx())
    buf->printf("(%lx)", sctxid);

  buf->printf(" ==> %lx ", dst);

  if (dst != dst_orig || lock_cnt)
    buf->printf("(");

  if (dst != dst_orig)
    buf->printf("want %lx", dst_orig);

  if (dst != dst_orig && lock_cnt)
    buf->printf(" ");

  if (lock_cnt)
    buf->printf("lck %lu", lock_cnt);

  if (dst != dst_orig || lock_cnt)
    buf->printf(") ");

  buf->printf(" krnl " L4_PTR_FMT " @ " L4_PTR_FMT, kernel_ip, _ip);
}


