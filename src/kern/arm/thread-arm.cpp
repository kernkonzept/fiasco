INTERFACE [arm]:

class Trap_state;

EXTENSION class Thread
{
public:
  static void init_per_cpu(Cpu_number cpu, bool resume);
private:
  bool _in_exception;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"
#include "kmem_space.h"
#include "mem_op.h"
#include "static_assert.h"
#include "thread_state.h"
#include "types.h"

enum {
  FSR_STATUS_MASK = 0x0d,
  FSR_TRANSL      = 0x05,
  FSR_DOMAIN      = 0x09,
  FSR_PERMISSION  = 0x0d,
};

DEFINE_PER_CPU Per_cpu<Thread::Dbg_stack> Thread::dbg_stack;

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  char const *const excpts[] =
    { "reset","undef. insn", "swi", "pref. abort", "data abort",
      "XXX", "XXX", "XXX" };

  unsigned ex = (e >> 20) & 0x07;

  printf("(%lx) %s, %s(%c)",e & 0xff, excpts[ex],
         (e & 0x00010000)?"user":"kernel",
         (e & 0x00020000)?'r':'w');
}

PUBLIC inline
void FIASCO_NORETURN
Thread::fast_return_to_user(Mword ip, Mword sp, Vcpu_state *arg)
{
  extern char __iret[];
  Entry_frame *r = regs();
  assert(r->check_valid_user_psr());

  r->ip(ip);
  r->sp(sp); // user-sp is in lazy user state and thus handled by
             // fill_user_state()
  fill_user_state();
  //load_tpidruro();

  r->psr &= ~Proc::Status_thumb;

  // extended vCPU runs the host code in ARM system mode
  if (Proc::Is_hyp && (state() & Thread_ext_vcpu_enabled))
    r->psr_set_mode(Proc::PSR_m_svc);

    {
      register Vcpu_state *r0 asm("r0") = arg;

      asm volatile
	("mov sp, %0  \t\n"
	 "mov pc, %1  \t\n"
	 :
	 : "r" (nonull_static_cast<Return_frame*>(r)), "r" (__iret), "r"(r0)
	);
    }
  panic("__builtin_trap()");
}

IMPLEMENT_DEFAULT inline
void
Thread::init_per_cpu(Cpu_number, bool)
{}


//
// Public services
//

IMPLEMENT
void
Thread::user_invoke()
{
  user_invoke_generic();
  assert (current()->state() & Thread_ready);

  Trap_state *ts = nonull_static_cast<Trap_state*>
    (nonull_static_cast<Return_frame*>(current()->regs()));

  assert (((Mword)ts & 7) == 4); // Return_frame has 5 words

  static_assert(sizeof(ts->r[0]) == sizeof(Mword), "Size mismatch");
  Mem::memset_mwords(&ts->r[0], 0, sizeof(ts->r) / sizeof(ts->r[0]));

  if (current()->space()->is_sigma0())
    ts->r[0] = Kmem_space::kdir()->virt_to_phys((Address)Kip::k());

  ts->psr |= Proc::Status_always_mask;

  extern char __return_from_user_invoke;

  asm volatile
    ("  mov sp, %[stack_p]    \n"    // set stack pointer to regs structure
     "  mov pc, %[rfe]        \n"
     :
     :
     [stack_p] "r" (ts),
     [rfe]     "r" (&__return_from_user_invoke)
     );

  panic("should never be reached");
  while (1)
    {
      current()->state_del(Thread_ready);
      current()->schedule();
    };

  // never returns here
}

IMPLEMENT inline NEEDS["space.h", "types.h", "config.h"]
bool Thread::handle_sigma0_page_fault(Address pfa)
{
  return mem_space()
    ->v_insert(Mem_space::Phys_addr((pfa & Config::SUPERPAGE_MASK)),
               Virt_addr(pfa & Config::SUPERPAGE_MASK),
               Virt_order(Config::SUPERPAGE_SHIFT) /*mem_space()->largest_page_size()*/,
               Mem_space::Attr(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}


PUBLIC static inline
bool
Thread::check_for_kernel_mem_access_pf(Trap_state *ts, Thread *t)
{
  if (EXPECT_FALSE(t->is_kernel_mem_op_hit_and_clear()))
    {
      Mword pc = t->exception_triggered() ? t->_exc_cont.ip() : ts->pc;

      pc -= (ts->psr & Proc::Status_thumb) ? 2 : 4;

      if (t->exception_triggered())
        t->_exc_cont.ip(pc);
      else
        ts->pc = pc;

      return true;
    }

  return false;
}


extern "C" {

  /**
   * The low-level page fault handler called from entry.S.  We're invoked with
   * interrupts turned off.  Apart from turning on interrupts in almost
   * all cases (except for kernel page faults in TCB area), just forwards
   * the call to Thread::handle_page_fault().
   * @param pfa page-fault virtual address
   * @param error_code CPU error code
   * @return true if page fault could be resolved, false otherwise
   */
  Mword pagefault_entry(const Mword pfa, Mword error_code,
                        const Mword pc, Return_frame *ret_frame)
  {
    if (EXPECT_FALSE(PF::is_alignment_error(error_code)))
      {
	printf("KERNEL%d: alignment error at %08lx (PC: %08lx, SP: %08lx, FSR: %lx, PSR: %lx)\n",
               cxx::int_value<Cpu_number>(current_cpu()), pfa, pc,
               ret_frame->usp, error_code, ret_frame->psr);
        return false;
      }

    if (EXPECT_FALSE(Thread::is_debug_exception(error_code, true)))
      return 0;

    Thread *t = current_thread();

    // cache operations we carry out for user space might cause PFs, we just
    // ignore those
    if (EXPECT_FALSE(!PF::is_usermode_error(error_code))
        && EXPECT_FALSE(t->is_ignore_mem_op_in_progress()))
      {
        t->set_kernel_mem_op_hit();
        ret_frame->pc += 4;
        return 1;
      }

    // Pagefault in user mode
    if (PF::is_usermode_error(error_code))
      {
        // PFs in the kern_lib_page are always write PFs due to rollbacks and
        // insn decoding
        if (EXPECT_FALSE((pc & Kmem::Kern_lib_base) == Kmem::Kern_lib_base))
          error_code |= (1UL << 6);

        // TODO: Avoid calling Thread::map_fsr_user here everytime!
        if (t->vcpu_pagefault(pfa, Thread::map_fsr_user(error_code, true), pc))
          return 1;
        t->state_del(Thread_cancel);
      }

    if (EXPECT_TRUE(PF::is_usermode_error(error_code))
        || !(ret_frame->psr & Proc::Status_preempt_disabled)
        || !Kmem::is_kmem_page_fault(pfa, error_code))
      Proc::sti();

    return t->handle_page_fault(pfa, error_code, pc, ret_frame);
  }

  void slowtrap_entry(Trap_state *ts)
  {
    ts->error_code = Thread::map_fsr_user(ts->error_code, false);

    if (0)
      printf("Trap: pfa=%08lx pc=%08lx err=%08lx psr=%lx\n", ts->pf_address, ts->pc, ts->error_code, ts->psr);
    Thread *t = current_thread();

    LOG_TRAP;

    if (Config::Support_arm_linux_cache_API)
      {
	if (   ts->esr.ec() == 0x11
            && ts->r[7] == 0xf0002)
	  {
            if (ts->r[2] == 0)
              Mem_op::arm_mem_cache_maint(Mem_op::Op_cache_coherent,
                                          (void *)ts->r[0], (void *)ts->r[1]);
            ts->r[0] = 0;
            return;
	  }
      }

    if (t->check_and_handle_coproc_faults(ts))
      return;

    if (Thread::is_debug_exception(ts->error_code))
      {
        Thread::handle_debug_exception(ts);
        return;
      }

    // send exception IPC if requested
    if (t->send_exception(ts))
      return;

    t->halt();
  }
};

PUBLIC static inline NEEDS[Thread::call_nested_trap_handler]
void
Thread::handle_debug_exception(Trap_state *ts)
{
  call_nested_trap_handler(ts);
}

IMPLEMENT inline
bool
Thread::pagein_tcb_request(Return_frame *regs)
{
  //if ((*(Mword*)regs->pc & 0xfff00fff ) == 0xe5900000)
  if (*(Mword*)regs->pc == 0xe59ee000)
    {
      // printf("TCBR: %08lx\n", *(Mword*)regs->pc);
      // skip faulting instruction
      regs->pc += 4;
      // tell program that a pagefault occurred we cannot handle
      regs->psr |= 0x40000000;	// set zero flag in psr
      regs->km_lr = 0;

      return true;
    }
  return false;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

PUBLIC static inline
Mword
Thread::is_debug_exception(Mword error_code, bool just_native_type = false)
{
  if (just_native_type)
    return (error_code & 0x4f) == 2;

  // LPAE type as already converted
  return (error_code & 0xc000003f) == 0x80000022;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC static inline
Mword
Thread::is_debug_exception(Mword error_code, bool just_native_type = false)
{
  if (just_native_type)
    return ((error_code >> 26) & 0x3f) == 0x22;
  return (error_code & 0xc000003f) == 0x80000022;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "trap_state.h"


/** Constructor.
    @param space the address space
    @param id user-visible thread ID of the sender
    @param init_prio initial priority
    @param mcp thread's maximum controlled priority
    @post state() != 0
 */
IMPLEMENT
Thread::Thread()
  : Sender(0),	// select optimized version of constructor
    _pager(Thread_ptr::Invalid),
    _exc_handler(Thread_ptr::Invalid),
    _del_observer(0)
{
  assert (state(false) == 0);

  inc_ref();
  _space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char *)this + sizeof(Thread), '5',
                Thread::Size - sizeof(Thread) - 64);

  // set a magic value -- we use it later to verify the stack hasn't
  // been overrun
  _magic = magic;
  _recover_jmpbuf = 0;
  _timeout = 0;
  _in_exception = false;

  *reinterpret_cast<void(**)()> (--_kernel_sp) = user_invoke;

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  r->sp(0);
  r->ip(0);
  r->psr = Proc::Status_mode_user;

  state_add_dirty(Thread_dead, false);

  // ok, we're ready to go!
}

IMPLEMENT inline
Mword
Thread::user_sp() const
{ return regs()->sp(); }

IMPLEMENT inline
void
Thread::user_sp(Mword sp)
{ return regs()->sp(sp); }


IMPLEMENT inline NEEDS[Thread::exception_triggered]
Mword
Thread::user_ip() const
{ return exception_triggered() ? _exc_cont.ip() : regs()->ip(); }

IMPLEMENT inline
Mword
Thread::user_flags() const
{ return 0; }

IMPLEMENT inline NEEDS[Thread::exception_triggered]
void
Thread::user_ip(Mword ip)
{
  if (exception_triggered())
    _exc_cont.ip(ip);
  else
    {
      Entry_frame *r = regs();
      r->ip(ip);
    }
}


PUBLIC inline NEEDS ["trap_state.h"]
int
Thread::send_exception_arch(Trap_state *)
{
  // nothing to tweak on ARM
  return 1;
}

PRIVATE inline
void
Thread::save_fpu_state_to_utcb(Trap_state *ts, Utcb *u)
{
  char *esu = (char *)&u->values[21];
  Fpu::save_user_exception_state(state() & Thread_fpu_owner,  fpu_state(),
                                 ts, (Fpu::Exception_state_user *)esu);
}

PRIVATE inline
bool
Thread::invalid_ipc_buffer(void const *a)
{
  if (!_in_exception)
    return Mem_layout::in_kernel(((Address)a & Config::SUPERPAGE_MASK)
                                 + Config::SUPERPAGE_SIZE - 1);

  return false;
}

PROTECTED inline
int
Thread::do_trigger_exception(Entry_frame *r, void *ret_handler)
{
  if (!_exc_cont.valid(r))
    {
      _exc_cont.activate(r, ret_handler);
      return 1;
    }
  return 0;
}


PRIVATE static inline NEEDS[Thread::get_ts_tpidruro]
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // if the message is too short just skip the whole copy in
  if (EXPECT_FALSE(tag.words() < (sizeof(Trap_state) / sizeof(Mword))))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
  Utcb *snd_utcb = snd->utcb().access();

  if (EXPECT_FALSE(rcv->exception_triggered()))
    {
      // triggered exception pending
      Mem::memcpy_mwords (ts, snd_utcb->values, 16);
      Return_frame rf = *reinterpret_cast<Return_frame const *>((char const *)&snd_utcb->values[16]);
      rcv->sanitize_user_state(&rf);
      rcv->_exc_cont.set(ts, &rf);
    }
  else
    rcv->copy_and_sanitize_trap_state(
       ts, reinterpret_cast<Trap_state const *>(snd_utcb->values));

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::W()))
    snd->transfer_fpu(rcv);

  // FIXME: this is an old l4linux specific hack, will be replaced/remved
  if ((tag.flags() & 0x8000) && (rights & L4_fpage::Rights::W()))
    rcv->utcb().access()->user[2] = snd_utcb->values[25];

  rcv->get_ts_tpidruro(ts);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}


PRIVATE static inline NEEDS[Thread::save_fpu_state_to_utcb,
                            Thread::set_ts_tpidruro]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();

    snd->set_ts_tpidruro(ts);

    Mem::memcpy_mwords(rcv_utcb->values, ts, 16);
    Continuation::User_return_frame *d
      = reinterpret_cast<Continuation::User_return_frame *>((char*)&rcv_utcb->values[16]);

    snd->_exc_cont.get(d, ts);


    if (EXPECT_TRUE(!snd->exception_triggered()))
      {
        rcv_utcb->values[19] = ts->pc;
        rcv_utcb->values[20] = ts->psr;
      }

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::W()))
      {
        snd->save_fpu_state_to_utcb(ts, rcv_utcb);
        snd->transfer_fpu(rcv);
      }
  }
  return true;
}

PROTECTED inline NEEDS[Thread::set_tpidruro]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb *utcb)
{
  switch (utcb->values[0] & Opcode_mask)
    {
    case Op_set_tpidruro_arm:
      return set_tpidruro(tag, utcb);
    default:
      return commit_result(-L4_err::ENosys);
    }
}

PROTECTED inline
int
Thread::sys_control_arch(Utcb *)
{
  return 0;
}

PUBLIC static inline
bool
Thread::condition_valid(unsigned char cond, Unsigned32 psr)
{
  // Matrix of instruction conditions and PSR flags,
  // index into the table is the condition from insn
  Unsigned16 v[16] =
  {
    0xf0f0,
    0x0f0f,
    0xcccc,
    0x3333,
    0xff00,
    0x00ff,
    0xaaaa,
    0x5555,
    0x0c0c,
    0xf3f3,
    0xaa55,
    0x55aa,
    0x0a05,
    0xf5fa,
    0xffff,
    0xffff
  };

  return (v[cond] >> (psr >> 28)) & 1;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_em_tz || hyp)]:

IMPLEMENT_OVERRIDE inline
bool
Thread::arch_ext_vcpu_enabled()
{ return true; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

PUBLIC static inline
Mword
Thread::map_fsr_user(Mword fsr, bool is_only_pf)
{
  static Unsigned16 const pf_map[32] =
  {
    /*  0x0 */ 0,
    /*  0x1 */ 0x21, /* Alignment */
    /*  0x2 */ 0x22, /* Debug */
    /*  0x3 */ 0x08, /* Access flag (1st level) */
    /*  0x4 */ 0x2000, /* Insn cache maint */
    /*  0x5 */ 0x04, /* Transl (1st level) */
    /*  0x6 */ 0x09, /* Access flag (2nd level) */
    /*  0x7 */ 0x05, /* Transl (2nd level) */
    /*  0x8 */ 0x10, /* Sync ext abort */
    /*  0x9 */ 0x3c, /* Domain (1st level) */
    /*  0xa */ 0,
    /*  0xb */ 0x3d, /* Domain (2nd level) */
    /*  0xc */ 0x14, /* Sync ext abt on PT walk (1st level) */
    /*  0xd */ 0x0c, /* Perm (1st level) */
    /*  0xe */ 0x15, /* Sync ext abt on PT walk (2nd level) */
    /*  0xf */ 0x0d, /* Perm (2nd level) */
    /* 0x10 */ 0x30, /* TLB conflict abort */
    /* 0x11 */ 0,
    /* 0x12 */ 0,
    /* 0x13 */ 0,
    /* 0x14 */ 0x34, /* Lockdown (impl-def) */
    /* 0x15 */ 0,
    /* 0x16 */ 0x11, /* Async ext abort */
    /* 0x17 */ 0,
    /* 0x18 */ 0x19, /* Async par err on mem access */
    /* 0x19 */ 0x18, /* Sync par err on mem access */
    /* 0x1a */ 0x3a, /* Copro abort (impl-def) */
    /* 0x1b */ 0,
    /* 0x1c */ 0x14, /* Sync par err on PT walk (1st level) */
    /* 0x1d */ 0,
    /* 0x1e */ 0x15, /* Sync par err on PT walk (2nd level) */
    /* 0x1f */ 0,
  };

  if (is_only_pf || (fsr & 0xc0000000) == 0x80000000)
    return pf_map[((fsr >> 10) & 1) | (fsr & 0xf)] | (fsr & ~0x43f);

  return fsr;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC static  inline
Mword
Thread::map_fsr_user(Mword fsr, bool)
{ return fsr; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && armv6plus]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{
  // just an experiment for now, we cannot really take the
  // user-writable register because user-land might already use it
  asm volatile("mcr p15, 0, %0, c13, c0, 2"
               : : "r" (utcb().access(true)->values[25]) : "memory");
}

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag tag, Utcb *utcb)
{
  if (EXPECT_FALSE(tag.words() < 2))
    return commit_result(-L4_err::EInval);

  _tpidruro = utcb->values[1];
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    arch_update_vcpu_state(vcpu_state().access());

  if (this == current_thread())
    load_tpidruro();

  return commit_result(0);
}

PRIVATE inline
void
Thread::get_ts_tpidruro(Trap_state *ts)
{
  _tpidruro = ts->tpidruro;
  if (this == current_thread())
    load_tpidruro();
}

PRIVATE inline
void
Thread::set_ts_tpidruro(Trap_state *ts)
{
  ts->tpidruro = _tpidruro;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !armv6plus]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{}

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag, Utcb *)
{
  return commit_result(-L4_err::EInval);
}

PRIVATE inline
void
Thread::get_ts_tpidruro(Trap_state *)
{}

PRIVATE inline
void
Thread::set_ts_tpidruro(Trap_state *)
{}

//-----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

#include "ipi.h"
#include "irq_mgr.h"

EXTENSION class Thread
{
public:
  static void kern_kdebug_ipi_entry() asm("kern_kdebug_ipi_entry");
};

class Thread_remote_rq_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_remote_requests_irq(); }

  Thread_remote_rq_irq()
  {
    set_hit(&handler_wrapper<Thread_remote_rq_irq>);
    unmask();
  }

  void switch_mode(bool) {}
};

class Thread_glbl_remote_rq_irq : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  { Thread::handle_global_remote_requests_irq(); }

  Thread_glbl_remote_rq_irq()
  {
    set_hit(&handler_wrapper<Thread_glbl_remote_rq_irq>);
    unmask();
  }

  void switch_mode(bool) {}
};

class Thread_debug_ipi : public Irq_base
{
public:
  // we assume IPIs to be top level, no upstream IRQ chips
  void handle(Upstream_irq const *)
  {
    Ipi::eoi(Ipi::Debug, current_cpu());
    Thread::kern_kdebug_ipi_entry();
  }

  Thread_debug_ipi()
  {
    set_hit(&handler_wrapper<Thread_debug_ipi>);
    unmask();
  }

  void switch_mode(bool) {}
};

class Thread_timer_tick_ipi : public Irq_base
{
public:
  void handle(Upstream_irq const *ui)
  {
    //Timer_tick *self = nonull_static_cast<Timer_tick *>(_s);
    //self->ack();
    ui->ack();
    //self->log_timer();
    current_thread()->handle_timer_interrupt();
  }

  Thread_timer_tick_ipi()
  { set_hit(&handler_wrapper<Thread_timer_tick_ipi>); }

  void switch_mode(bool) {}
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [mp && !irregular_gic]:

class Arm_ipis
{
public:
  Arm_ipis()
  {
    check(Irq_mgr::mgr->alloc(&remote_rq_ipi, Ipi::Request));
    check(Irq_mgr::mgr->alloc(&glbl_remote_rq_ipi, Ipi::Global_request));
    check(Irq_mgr::mgr->alloc(&debug_ipi, Ipi::Debug));
    check(Irq_mgr::mgr->alloc(&timer_ipi, Ipi::Timer));
  }

  Thread_remote_rq_irq remote_rq_ipi;
  Thread_glbl_remote_rq_irq glbl_remote_rq_ipi;
  Thread_debug_ipi debug_ipi;
  Thread_timer_tick_ipi timer_ipi;
};

static Arm_ipis _arm_ipis;

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && !fpu]:

PUBLIC inline
bool
Thread::check_and_handle_coproc_faults(Trap_state *)
{
  return false;
}


//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu]:

PUBLIC inline
bool
Thread::check_and_handle_coproc_faults(Trap_state *ts)
{
  if (!ts->exception_is_undef_insn())
    return false;

  Unsigned32 opcode;

  if (ts->psr & Proc::Status_thumb)
    {
      Unsigned16 v = Thread::peek_user((Unsigned16 *)(ts->pc - 2), this);

      if (EXPECT_FALSE(Thread::check_for_kernel_mem_access_pf(ts, this)))
        return true;

      if ((v >> 11) <= 0x1c)
        return false;

      opcode = (v << 16) | Thread::peek_user((Unsigned16 *)ts->pc, this);
    }
  else
    opcode = Thread::peek_user((Unsigned32 *)(ts->pc - 4), this);

  if (EXPECT_FALSE(Thread::check_for_kernel_mem_access_pf(ts, this)))
    return true;

  if (ts->psr & Proc::Status_thumb)
    {
      if (   (opcode & 0xef000000) == 0xef000000 // A6.3.18
          || (opcode & 0xff100000) == 0xf9000000)
        return Thread::handle_fpu_trap(opcode, ts);
    }
  else
    {
      if (   (opcode & 0xfe000000) == 0xf2000000 // A5.7.1
          || (opcode & 0xff100000) == 0xf4000000)
        return Thread::handle_fpu_trap(opcode, ts);
    }

  if ((opcode & 0x0c000e00) == 0x0c000a00)
    return Thread::handle_fpu_trap(opcode, ts);

  return false;
}

PUBLIC static
bool
Thread::handle_fpu_trap(Unsigned32 opcode, Trap_state *ts)
{
  if (!condition_valid(opcode >> 28, ts->psr))
    {
      // FPU insns are 32bit, even for thumb
      if (ts->psr & Proc::Status_thumb)
        ts->pc += 2;
      return true;
    }

  if (Fpu::is_enabled())
    {
      assert(Fpu::fpu.current().owner() == current());
      if (Fpu::is_emu_insn(opcode))
        return Fpu::emulate_insns(opcode, ts);

      ts->esr.ec() = 0; // tag fpu undef insn
    }
  else if (current_thread()->switchin_fpu())
    {
      if (Fpu::is_emu_insn(opcode))
        return Fpu::emulate_insns(opcode, ts);
      ts->pc -= (ts->psr & Proc::Status_thumb) ? 2 : 4;
      return true;
    }
  else
    {
      ts->esr.ec() = 0x07;
      ts->esr.cond() = opcode >> 28;
      ts->esr.cv() = 1;
      ts->esr.cpt_cpnr() = 10;
    }

  return false;
}


//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && !hyp]:

PUBLIC static inline template<typename T>
T Thread::peek_user(T const *adr, Context *c)
{
  T v;
  c->set_ignore_mem_op_in_progress(true);
  v = *adr;
  c->set_ignore_mem_op_in_progress(false);
  return v;
}


