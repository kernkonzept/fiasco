INTERFACE [arm]:

class Trap_state;

EXTENSION class Thread
{
public:
  static void init_per_cpu(Cpu_number cpu, bool resume);
  bool check_and_handle_coproc_faults(Trap_state *);

private:
  bool _in_exception;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"
#include "kmem.h"
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

PUBLIC inline NEEDS[Thread::arm_fast_exit]
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

  // masking the illegal execution bit does not harm
  // on 32bit it is res/sbz
  r->psr &= ~(Proc::Status_thumb | (1UL << 20));

  // extended vCPU runs the host code in ARM system mode
  if (Proc::Is_hyp && (state() & Thread_ext_vcpu_enabled))
    r->psr_set_mode(Proc::Status_mode_vmm);

  arm_fast_exit(nonull_static_cast<Return_frame*>(r), __iret, arg);
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

  auto *ct = current_thread();
  auto *regs = ct->regs();
  Trap_state *ts = nonull_static_cast<Trap_state*>
    (nonull_static_cast<Return_frame*>(regs));

  static_assert(sizeof(ts->r[0]) == sizeof(Mword), "Size mismatch");
  Mem::memset_mwords(&ts->r[0], 0, sizeof(ts->r) / sizeof(ts->r[0]));

  if (ct->space()->is_sigma0())
    ts->r[0] = Kmem::kdir->virt_to_phys((Address)Kip::k());

  if (ct->exception_triggered())
    ct->_exc_cont.flags(regs, ct->_exc_cont.flags(regs)
                              | Proc::Status_always_mask);
  else
    regs->psr |= Proc::Status_always_mask;
  Proc::cli();
  extern char __return_from_user_invoke[];
  arm_fast_exit(ts, __return_from_user_invoke, ts);
  panic("should never be reached");
  while (1)
    {
      ct->state_del(Thread_ready);
      ct->schedule();
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
        error_code = Thread::mangle_kernel_lib_page_fault(pc, error_code);

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
Thread::Thread(Ram_quota *q)
: Sender(0),
  _pager(Thread_ptr::Invalid),
  _exc_handler(Thread_ptr::Invalid),
  _quota(q),
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

  prepare_switch_to(&user_invoke);

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  memset(r, 0, sizeof(*r));
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

IMPLEMENT inline
Mword
Thread::user_flags() const
{ return 0; }

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

PROTECTED inline
int
Thread::sys_control_arch(Utcb *)
{
  return 0;
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

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_em_tz || cpu_virt)]:

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
IMPLEMENTATION [arm && 32bit && arm_v6plus]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{
  // just an experiment for now, we cannot really take the
  // user-writable register because user-land might already use it
  asm volatile("mcr p15, 0, %0, c13, c0, 2"
               : : "r" (utcb().access(true)->values[25]) : "memory");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (64bit || !arm_v6plus)]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

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
Thread::set_tpidruro(Trex const *t)
{
  _tpidruro = access_once(&t->tpidruro);
  if (this == current_thread())
    load_tpidruro();
}

PRIVATE inline
void
Thread::store_tpidruro(Trex *t)
{
  t->tpidruro = _tpidruro;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag, Utcb *)
{
  return commit_result(-L4_err::EInval);
}

PRIVATE inline
void
Thread::set_tpidruro(Trex const *)
{}

PRIVATE inline
void
Thread::store_tpidruro(Trex *)
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

PUBLIC static inline
void
Thread::handle_debug_remote_requests_irq()
{
  Ipi::eoi(Ipi::Debug, current_cpu());
  Thread::kern_kdebug_ipi_entry();
}

PUBLIC static inline
void
Thread::handle_timer_remote_requests_irq(Upstream_irq const *ui)
{
  ui->ack();
  current_thread()->handle_timer_interrupt();
}

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
  { Thread::handle_debug_remote_requests_irq(); }

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
  { Thread::handle_timer_remote_requests_irq(ui); }

  Thread_timer_tick_ipi()
  { set_hit(&handler_wrapper<Thread_timer_tick_ipi>); }

  void switch_mode(bool) {}
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [mp && !arm_single_ipi_irq && !irregular_gic]:

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
IMPLEMENTATION [arm]:

IMPLEMENT_DEFAULT inline
bool
Thread::check_and_handle_coproc_faults(Trap_state *)
{
  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC static inline template<typename T>
T Thread::peek_user(T const *adr, Context *c)
{
  T v;
  c->set_ignore_mem_op_in_progress(true);
  v = *adr;
  c->set_ignore_mem_op_in_progress(false);
  return v;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_esr_traps]:

EXTENSION class Thread
{
  static void arm_esr_entry(Return_frame *rf) asm ("arm_esr_entry");
};

IMPLEMENT
void
Thread::arm_esr_entry(Return_frame *rf)
{
  Trap_state *ts = static_cast<Trap_state*>(rf);
  Thread *ct = current_thread();

  Arm_esr esr = Thread::get_esr();
  ts->error_code = esr.raw();

  Mword tmp;
  Mword state = ct->state();

  switch (esr.ec())
    {
    case 0x20:
      tmp = get_fault_pfa(esr, true, state & Thread_ext_vcpu_enabled);
      if (!pagefault_entry(tmp, esr.raw(), rf->pc, rf))
        {
          Proc::cli();
          ts->pf_address = tmp;
          slowtrap_entry(ts);
        }
      Proc::cli();
      return;

    case 0x24:
      tmp = get_fault_pfa(esr, false, state & Thread_ext_vcpu_enabled);
      if (!pagefault_entry(tmp, esr.raw(), rf->pc, rf))
        {
          Proc::cli();
          ts->pf_address = tmp;
          slowtrap_entry(ts);
        }
      Proc::cli();
      return;

    case 0x12: // HVC
    case 0x11: // SVC
    case 0x15: // SVC from aarch64
      current_thread()->handle_svc(ts);
      return;

    case 0x00: // undef opcode with HCR.TGE=1
        {
          ct->state_del(Thread_cancel);
          Mword state = ct->state();
          Unsigned32 pc = rf->pc;

          if (state & (Thread_vcpu_user | Thread_alien))
            {
              ts->pc += ts->psr & Proc::Status_thumb ? 2 : 4;
              ct->send_exception(ts);
              return;
            }
          else if (EXPECT_FALSE(!is_syscall_pc(pc + 4)))
            {
              ts->pc += ts->psr & Proc::Status_thumb ? 2 : 4;
              slowtrap_entry(ts);
              return;
            }

          rf->pc = get_lr_for_mode(rf);
          ct->state_del(Thread_cancel);
          typedef void Syscall(void);
          extern Syscall *sys_call_table[];
          sys_call_table[-(pc + 4) / 4]();
          return;
        }
      break;

    case 0x07:
      if ((sizeof(Mword) == 8
           || esr.cpt_simd()
           || esr.cpt_cpnr() == 10
           || esr.cpt_cpnr() == 11)
          && Thread::handle_fpu_trap(ts))
        return;

      ct->send_exception(ts);
      break;

    case 0x03: // CP15 trapped
      if (esr.mcr_coproc_register() == esr.mrc_coproc_register(0, 1, 0, 1))
        {
          ts->r[esr.mcr_rt()] = 1 << 6;
          ts->pc += 2 << esr.il();
          return;
        }
      // fall through

    default:
      ct->send_exception(ts);
      break;
    }
}

