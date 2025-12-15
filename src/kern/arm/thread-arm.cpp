INTERFACE [arm]:

class Trap_state;

EXTENSION class Thread
{
public:
  enum Ex_regs_flags_arm
  {
    Exr_arm_set_el_mask     = 0x3UL << 24,
    Exr_arm_set_el_keep     = 0x0UL << 24,
    Exr_arm_set_el_el0      = 0x1UL << 24,
    Exr_arm_set_el_el1      = 0x2UL << 24,

    Exr_arm_unassigned    = Exr_arch_mask & ~Exr_arm_set_el_mask,
  };

  static void init_per_cpu(Cpu_number cpu, bool resume);
  static bool check_and_handle_linux_cache_api(Trap_state *);
  bool check_and_handle_mem_op_fault(Mword error_code, Return_frame *ret_frame);
  bool check_and_handle_coproc_faults(Trap_state *);

private:
  bool handle_sve_trap(Trap_state *);
};

// ------------------------------------------------------------------------
INTERFACE [arm && mmu]:

#include "config.h"
#include "paging_bits.h"

EXTENSION class Thread
{
  enum : unsigned { Sigma0_pf_page_shift = Config::SUPERPAGE_SHIFT };

  typedef Super_pg Sigma0_pg;
};

// ------------------------------------------------------------------------
INTERFACE [arm && !mmu]:

#include "config.h"
#include "paging_bits.h"

EXTENSION class Thread
{
  // Must not map too much on no-MMU systems because sigma0 is usually
  // adjacent to Fiasco but the MPU regions must *not* intersect! Otherwise
  // a translation fault is triggered.
  enum : unsigned { Sigma0_pf_page_shift = Config::PAGE_SHIFT };

  typedef Pg Sigma0_pg;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"
#include "kmem.h"
#include "thread_state.h"
#include "trap_state.h"
#include "types.h"
#include "warn.h"

enum {
  FSR_STATUS_MASK = 0x0d,
  FSR_TRANSL      = 0x05,
  FSR_DOMAIN      = 0x09,
  FSR_PERMISSION  = 0x0d,
};

PUBLIC inline NEEDS[Thread::arm_fast_exit]
[[noreturn]] void
Thread::vcpu_return_to_kernel(Mword ip, Mword sp, Vcpu_state *arg)
{
  Return_frame *r = prepare_vcpu_return_to_kernel(ip, sp);

  extern char __iret[];
  arm_fast_exit(r, __iret, arg);

  // never returns here
}

/**
 * Prepare return frame for vCPU kernel mode entry.
 *
 * As a special optimization this is called from AArch32 assembly to optimize
 * the switch on this architecture. AArch64 always utilizes
 * Thread::vcpu_return_to_kernel().
 */
PUBLIC inline
Return_frame *
Thread::prepare_vcpu_return_to_kernel(Mword ip, Mword sp)
{
  Entry_frame *r = regs();

  r->ip(ip);
  r->sp(sp); // user-sp is in lazy user state and thus handled by
             // fill_user_state()
  fill_user_state();
  //load_tpidruro();

  // masking the illegal execution bit does not harm
  // on 32bit it is res/sbz
  r->psr &= ~(Proc::Status_thumb | (1UL << 20));

  // make sure the VMM executes in the correct mode
  sanitize_vmm_state(r);
  assert(r->check_valid_user_psr());

  return nonull_static_cast<Return_frame*>(r);
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
  Mem::memset_mwords(&ts->r[0], 0, cxx::size(ts->r));

  if (ct->space()->is_sigma0())
    ts->r[0] = Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(Kip::k()));

  // load KIP syscall code into r1/x1 to allow user processes to
  // do syscalls even without access to the KIP.
  ts->r[1] = reinterpret_cast<Mword *>(Kip::k())[0x100];

  if (ct->exception_triggered())
    ct->_exc_cont.flags(regs, ct->_exc_cont.flags(regs)
                              | Proc::Status_always_mask);
  else
    regs->psr |= Proc::Status_always_mask;
  Proc::cli();
  extern char __return_from_user_invoke[];
  arm_fast_exit(ts, __return_from_user_invoke, ts);

  // never returns here
}

IMPLEMENT inline NEEDS["space.h", "types.h", "config.h", "paging_bits.h"]
bool Thread::handle_sigma0_page_fault(Address pfa)
{
  return mem_space()
    ->v_insert(Mem_space::Phys_addr(Sigma0_pg::trunc(pfa)),
               Virt_addr(Sigma0_pg::trunc(pfa)),
               Virt_order(Sigma0_pf_page_shift),
               Mem_space::Attr::space_local(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}


extern "C" {
  Mword pagefault_entry(const Mword pfa, Mword error_code,
                        const Mword pc, Return_frame *ret_frame);
  void slowtrap_entry(Trap_state *ts);

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
	WARNX(Warning,
              "KERNEL%d: alignment error at %08lx (PC: %08lx, SP: %08lx, FSR: %lx, PSR: %lx)\n",
              cxx::int_value<Cpu_number>(current_cpu()), pfa, pc,
              ret_frame->usp, error_code, ret_frame->psr);
        return false;
      }

    if (EXPECT_FALSE(Thread::is_debug_exception_fsr(error_code)))
      return 0;

    Thread *t = current_thread();

    if (EXPECT_FALSE(t->check_and_handle_mem_op_fault(error_code, ret_frame)))
      return 1;

    // Pagefault in user mode
    if (PF::is_usermode_error(error_code))
      {
        error_code = Thread::mangle_kernel_lib_page_fault(pc, error_code);

        // TODO: Avoid calling Thread::map_fsr_user here everytime!
        if (t->vcpu_pagefault(pfa, Thread::map_fsr_user(error_code), pc))
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
    if (Thread::is_fsr_exception(ts->esr))
      ts->error_code = Thread::map_fsr_user(ts->error_code);

    if constexpr (0) // Intentionally disabled, only used for diagnostics
      printf("Trap: pfa=%08lx pc=%08lx err=%08lx psr=%lx\n", ts->pf_address, ts->pc, ts->error_code, ts->psr);
    Thread *t = current_thread();

    LOG_TRAP;

    if (t->check_and_handle_linux_cache_api(ts))
      return;

    if (t->check_and_handle_coproc_faults(ts))
      return;

    if (Thread::is_debug_exception(ts->esr))
      {
        t->handle_debug_exception(ts);
        return;
      }

    // send exception IPC if requested
    if (t->send_exception(ts))
      return;

    t->kill();
  }
};


/** Constructor.
    @post state() != 0
 */
IMPLEMENT
Thread::Thread(Ram_quota *q)
: Sender(0),
  _pager(Thread_ptr::Invalid),
  _exc_handler(Thread_ptr::Invalid),
  _quota(q)
{
  assert (state(false) == 0);

  inc_ref();
  _space.space(Kernel_task::kernel_task());

  if constexpr (Config::Stack_depth)
    std::memset(reinterpret_cast<char *>(this) + sizeof(Thread), '5',
                Thread::Size - sizeof(Thread) - 64);

  // set a magic value -- we use it later to verify the stack hasn't
  // been overrun
  _magic = magic;
  _timeout = nullptr;
  clear_recover_jmpbuf();

  prepare_switch_to(&user_invoke);

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  memset(r, 0, sizeof(*r));
  r->psr = Proc::Status_mode_user;

  alloc_eager_fpu_state();
  init_mpu_state();

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

/*
 * L4-IFACE: kernel-thread.thread-set_tpidruro_arm
 * PROTOCOL: L4_PROTO_THREAD
 */
PROTECTED inline NEEDS[Thread::set_tpidruro]
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb const *utcb, Utcb *)
{
  switch (Op{utcb->values[0] & Opcode_mask})
    {
    case Op::Set_tpidruro_arm:
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
IMPLEMENTATION [arm && !(arm_lpae || mpu)]:

/**
 * Map from Short-descriptor FSR format (FS[10, 3:0]) to Long-descriptor FSR
 * format (STATUS[5:0]).
 *
 * In order to provide consistent error bits to user-space regardless of the
 * underlying CPU type, Fiasco exposes a HSR based error description to
 * user-mode, even if Hyp mode is not enabled/available. Thus, the non-Hyp mode
 * exception vector already partially translates the error description reported
 * by the hardware in the (I/D)FSR register into an HSR error description.
 * Hence, the `fsr` argument received by this method is a mixture of (I/D)FSR
 * and HSR encodings, for example HSR.EC, HSR.ISS.WnR were already translated,
 * but FS[10, 3:0] (and also other bits) were passed on untouched.
 *
 * This method only takes care of mapping the fault status. The other (I/F)FSR
 * bits, are currently not translated into the corresponding HSR bits. The
 * assumption is that these bits are not tested anywhere, and therefore it would
 * not be worth the effort.
 *
 * The encoding of the fault status code ISS.(I/D)FSC in HSR is equivalent to
 * the fault status of the Long-descriptor FSR format. Therefore, only for the
 * short-descriptor FSR format a translation of the fault status code is
 * necessary.
 */
PUBLIC static inline
Mword
Thread::map_fsr_user(Mword fsr)
{
  static Unsigned16 const pf_map[32] =
  {
    /*  0x0 */ 0,
    /*  0x1 */ 0x21, /* Alignment */
    /*  0x2 */ 0x22, /* Debug */
    /*  0x3 */ 0x08, /* Access flag (1st level) */
    /*  0x4 */ 0,    /* Insn cache maint */
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

  enum
  {
    // Fault status (Short-descriptor FSR format): FS[10, 3:0]
    Fsr_fs_mask      = 0x40f,
    // Fault status (Long-descriptor FSR format): STATUS[5:0]
    Fsr_status_mask  = 0x3f,
  };

  unsigned fs = ((fsr >> 6) & 0x10) | (fsr & 0xf); // Combine FS bits
  return pf_map[fs] | (fsr & ~(Fsr_fs_mask | Fsr_status_mask));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_lpae || mpu)]:

PUBLIC static  inline
Mword
Thread::map_fsr_user(Mword fsr)
{ return fsr; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag tag, Utcb const *utcb)
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
Thread::set_tpidrurw(Trex const *t)
{
  _tpidrurw = access_once(&t->tpidrurw);
  if (this == current_thread())
    load_tpidrurw();
}

PRIVATE inline
void
Thread::store_tpidruro(Trex *t)
{
  t->tpidruro = _tpidruro;
}

PRIVATE inline
void
Thread::store_tpidrurw(Trex *t)
{
  // If this thread is the current thread, the value stored in the _tpidrurw
  // field might be outdated, as the _tpidrurw field is only updated when
  // switching away from a thread. So for a thread that recently changed its
  // TPIDRURW register, the new value will not yet be reflected in the _tpidrurw
  // field if the thread was not switched away from since the change.
  if (this == current_thread())
    // Ensure the _tpidrurw field is up-to-date with the TPIDRURW register.
    Context::store_tpidrurw();
  t->tpidrurw = _tpidrurw;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

PRIVATE inline
L4_msg_tag
Thread::set_tpidruro(L4_msg_tag, Utcb const *)
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

PUBLIC static inline NEEDS["ipi.h"]
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
  Upstream_irq::ack(ui);
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

  void switch_mode(bool) override {}
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

  void switch_mode(bool) override {}
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

  void switch_mode(bool) override {}
};

class Thread_timer_tick_ipi : public Irq_base
{
public:
  void handle(Upstream_irq const *ui)
  { Thread::handle_timer_remote_requests_irq(ui); }

  Thread_timer_tick_ipi()
  { set_hit(&handler_wrapper<Thread_timer_tick_ipi>); }

  void switch_mode(bool) override {}
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [mp && !arm_single_ipi_irq && !irregular_gic]:

class Arm_ipis
{
public:
  Arm_ipis()
  {
    check(Irq_mgr::mgr->gsi_attach(&remote_rq_ipi, Ipi::Request, false));
    check(Irq_mgr::mgr->gsi_attach(&glbl_remote_rq_ipi, Ipi::Global_request, false));
    check(Irq_mgr::mgr->gsi_attach(&debug_ipi, Ipi::Debug, false));
    check(Irq_mgr::mgr->gsi_attach(&timer_ipi, Ipi::Timer, false));
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
Thread::check_and_handle_linux_cache_api(Trap_state *)
{
  return false;
}

IMPLEMENT_DEFAULT inline
bool
Thread::check_and_handle_mem_op_fault(Mword, Return_frame *)
{
  return false;
}

IMPLEMENT_DEFAULT inline
bool
Thread::check_and_handle_coproc_faults(Trap_state *)
{
  return false;
}

IMPLEMENT_DEFAULT inline
bool
Thread::handle_sve_trap(Trap_state *)
{
  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
Mword
Thread::user_flags() const
{ return 0; }

IMPLEMENT_OVERRIDE
bool
Thread::ex_regs_arch(Mword ops)
{
  if (ops & Exr_arm_unassigned)
    return false;

  switch (ops & Exr_arm_set_el_mask)
    {
    case Exr_arm_set_el_keep:
    case Exr_arm_set_el_el0:
      return true;
    }

  return false;
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
    case 0x20: // Instruction abort from a lower exception level
      tmp = get_fault_pfa(esr, true, state & Thread_ext_vcpu_enabled);
      if (!pagefault_entry(tmp, esr.raw(), rf->pc, rf))
        {
          Proc::cli();
          ts->pf_address = tmp;
          slowtrap_entry(ts);
        }
      Proc::cli();
      return;

    case 0x24: // Data abort from a lower exception Level
      tmp = get_fault_pfa(esr, false, state & Thread_ext_vcpu_enabled);
      if (!pagefault_entry(tmp, esr.raw(), rf->pc, rf))
        {
          Proc::cli();
          ts->pf_address = tmp;
          slowtrap_entry(ts);
        }
      Proc::cli();
      return;

    case 0x11: // SVC instruction execution on AArch32
    case 0x12: // HVC instruction execution on AArch32
    case 0x15: // SVC instruction execution on AArch64
    case 0x16: // HVC instruction execution on AArch64
      current_thread()->handle_svc(ts);
      return;

    case 0x00: // Unknown reason, undefined opcode with HCR.TGE=1
        {
          ct->state_del(Thread_cancel);
          Mword state = ct->state();

          if (state & (Thread_vcpu_user | Thread_alien))
            {
              ct->send_exception(ts);
              return;
            }

          slowtrap_entry(ts);
          return;
        }

    case 0x07: // SVE, Advanced SIMD or floating-point trap
      if ((TAG_ENABLED(arm_v8) // Always FPU trap on Armv8, not used for other CPs.
           || esr.cpt_simd() == 1
           || esr.cpt_cpnr() == 10  // CP10: Floating-point
           || esr.cpt_cpnr() == 11) // CP11: Advanced SIMD
          && Thread::handle_fpu_trap(ts))
        return;

      ct->send_exception(ts);
      break;

    case 0x19: // SVE
      if (ct->handle_sve_trap(ts))
        return;

      ct->send_exception(ts);
      break;

    default:
      ct->send_exception(ts);
      break;
    }
}

