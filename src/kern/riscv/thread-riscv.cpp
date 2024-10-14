INTERFACE [riscv]:

EXTENSION class Thread
{
public:
  static int
  thread_handle_trap(Mword cause, Mword val,
                     Return_frame *ret_frame) asm ("thread_handle_trap");

  static int
  thread_handle_slow_trap(Trap_state *ts) asm ("thread_handle_slow_trap");
};

typedef void (*Handler)(Mword cause, Mword epc, Mword val);

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "cpu.h"
#include "pic.h"
#include "sbi.h"
#include "trap_state.h"
#include "types.h"
#include "paging_bits.h"

#include "warn.h"
#include <cassert>

IMPLEMENT inline NEEDS[Thread::exception_triggered]
Mword
Thread::user_ip() const
{ return regs()->ip(); }

IMPLEMENT inline NEEDS[Thread::exception_triggered]
void
Thread::user_ip(Mword ip)
{ regs()->ip(ip); }

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

  if constexpr (Config::Stack_depth)
    std::memset(reinterpret_cast<char *>(this) + sizeof(Thread), '5',
                Thread::Size - sizeof(Thread) - 64);

  // set a magic value -- we use it later to verify the stack hasn't
  // been overrun
  _magic = magic;
  _timeout = 0;

  prepare_switch_to(&user_invoke);

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  memset(r, 0, sizeof(*r));
  // return to user-mode with interrupts enabled.
  r->status = Cpu::Sstatus_user_default;
  r->init_hstatus();

  alloc_eager_fpu_state();

  state_add_dirty(Thread_dead, false);

  // ok, we're ready to go!
}

PUBLIC [[noreturn]] inline
void
Thread::vcpu_return_to_kernel(Mword ip, Mword sp, void *arg)
{
  Entry_frame *r = regs();
  assert(r->user_mode());

  r->ip(ip);
  r->sp(sp);

  assert(!(_state & Thread_vcpu_user));
  // On RISC-V, in addition to ip and sp, also the gp and tp registers
  // are required for a minimal execution environment.
  vcpu_restore_host_regs(r);

  assert(cpu_lock.test());
  extern char fast_sret[];
  riscv_fast_exit(nonull_static_cast<Return_frame*>(r), fast_sret, arg);

  // never returns here
}

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

  if (ct->space()->is_sigma0())
    ts->arg0(Kmem::kdir->virt_to_phys(reinterpret_cast<Address>(Kip::k())));

  Proc::cli();
  extern char return_from_user_invoke[];
  riscv_fast_exit(ts, return_from_user_invoke, ts);

  // never returns here
}

IMPLEMENT inline NEEDS["space.h", "types.h", "config.h", "paging_bits.h"]
bool
Thread::handle_sigma0_page_fault(Address pfa)
{
  bool success = mem_space()
    ->v_insert(Mem_space::Phys_addr(Super_pg::trunc(pfa)),
               Virt_addr(Super_pg::trunc(pfa)),
               Virt_order(Config::SUPERPAGE_SHIFT) /*mem_space()->largest_page_size()*/,
               Mem_space::Attr::space_local(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;

  if (Mem_space::Need_insert_tlb_flush)
    mem_space()->tlb_flush_current_cpu();

  return success;
}

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  printf("%s%s (%lx), %s(%c)",
         PF::is_instruction_error(e)
           ? "inst" : (PF::is_read_error(e) ? "load" : "store"),
         PF::is_translation_error(e) ? "" : " access", e,
         PF::is_usermode_error(e) ? "user" : "kernel",
         PF::is_read_error(e) ? 'r' : 'w');
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
Thread::sys_control_arch(Utcb const *, Utcb *)
{
  return 0;
}

PROTECTED inline
L4_msg_tag
Thread::invoke_arch(L4_msg_tag, Utcb *, Utcb *)
{
  return commit_result(-L4_err::ENosys);
}

PRIVATE [[nodiscard]] static inline
bool
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // only a complete state will be used.
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = reinterpret_cast<Trap_state*>(rcv->_utcb_handler);
  Utcb *snd_utcb = snd->utcb().access();

  Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
  ts->copy_and_sanitize(&r->s);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PRIVATE [[nodiscard]] static inline
bool
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = reinterpret_cast<Trap_state*>(snd->_utcb_handler);
  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *r = reinterpret_cast<Trex *>(rcv_utcb->values);
    r->s = *ts;

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);
  }

  return true;
}

PUBLIC [[noreturn]] static inline
void
Thread::riscv_fast_exit(void *sp, void *pc, void *arg)
{
  register void *a0 asm("a0") = arg;
  __asm__ __volatile__ (
    "  mv   sp, %[stack] \n"
    "  jr   %[rfe]       \n"
    : :
    [stack]   "r" (sp),
    [rfe]     "r" (pc),
              "r" (a0));

  __builtin_unreachable();
}

extern "C" void leave_by_vcpu_upcall(Trap_state *ts)
{
  Thread *c = current_thread();
  Vcpu_state *vcpu = c->vcpu_state().access();
  Mem::memcpy_mwords(vcpu->_regs.s.regs, ts->regs,
                     sizeof(ts->regs) / sizeof(ts->regs[0]));
  vcpu->_regs.s._pc = ts->_pc;
  vcpu->_regs.s.status = ts->status;
  vcpu->_regs.s.copy_hstatus(ts);

  c->vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_sp,
                           c->vcpu_state().usr().get());
}

IMPLEMENT
int
Thread::thread_handle_trap(Mword cause, Mword val, Return_frame *ret_frame)
{
  assert(cpu_lock.test());

  Thread *t = current_thread();

  // Handle hypervisor traps
  if (t->handle_hyp_ext_trap(cause, val, ret_frame))
    {
      assert(cpu_lock.test());
      return 1;
    }

  int result = 0;
  switch (cause)
    {
    // Supervisor timer interrupt
    case Cpu::Int_supervisor_timer:
      result = t->handle_timer_interrupt_riscv();
      break;

    case Cpu::Int_supervisor_software:
      result = t->handle_software_interrupt();
      break;

    case Cpu::Int_supervisor_external:
      result = t->handle_external_interrupt();
      break;

    // Page fault
    case Cpu::Exc_inst_page_fault:
    case Cpu::Exc_load_page_fault:
    case Cpu::Exc_store_page_fault:
      result = t->handle_page_fault_riscv(cause, val, ret_frame);
      if (!result)
        {
          result = t->handle_slow_trap(static_cast<Trap_state *>(ret_frame));
        }
      break;

    // Environment call
    case Cpu::Exc_ecall:
      result = t->handle_ecall(ret_frame);
      break;

    // Illegal instruction
    case Cpu::Exc_illegal_inst:
      result = t->handle_fpu_trap(ret_frame);
      break;

    // Breakpoint
    case Cpu::Exc_breakpoint:
      result = t->handle_breakpoint(ret_frame);
      break;

    // PMA/PMP check failed
    case Cpu::Exc_inst_access:
    case Cpu::Exc_load_acesss:
    case Cpu::Exc_store_acesss:

    default:
      WARN("Unhandled Trap: Cause=" L4_MWORD_FMT ", Epc=" L4_MWORD_FMT ", Val=" L4_MWORD_FMT "\n",
           cause, ret_frame->ip(), val);
      result = t->handle_slow_trap(static_cast<Trap_state *>(ret_frame));
    }

    if (!result)
      WARN("Trap handling failed: Cause=" L4_MWORD_FMT ", Epc=" L4_MWORD_FMT ", Val=" L4_MWORD_FMT "\n",
           cause, ret_frame->ip(), val);

    assert(cpu_lock.test());
    return result;
}

PROTECTED inline
int
Thread::handle_timer_interrupt_riscv()
{
  Timer::handle_interrupt();
  handle_timer_interrupt();

  return 1;
}

PROTECTED inline
int
Thread::handle_software_interrupt()
{
  return handle_ipi();
}

PROTECTED inline NEEDS["pic.h"]
int
Thread::handle_external_interrupt()
{
  Pic::handle_interrupt();
  return 1;
}

PROTECTED inline
int
Thread::handle_page_fault_riscv(Mword cause, Mword pfa, Return_frame *ret_frame)
{
  // RISC-V does not indicate whether a page fault was caused
  // by a missing mapping or by insufficient permissions.
  Mword error_code = 0;

  if (ret_frame->user_mode())
    error_code |= PF::Err_usermode;

  switch (cause)
    {
    case Cpu::Exc_inst_page_fault:
      error_code |= PF::Err_inst;
      break;

    case Cpu::Exc_load_page_fault:
      error_code |= PF::Err_load;
      break;

    case Cpu::Exc_store_page_fault:
      error_code |= PF::Err_store;
      break;
    }

  // Pagefault in user mode
  if (EXPECT_TRUE(PF::is_usermode_error(error_code))
      && vcpu_pagefault(pfa, cause, ret_frame->ip()))
    return 1;

  // Enable interrupts, except for kernel page faults in TCB area.
  Lock_guard<Cpu_lock, Lock_guard_inverse_policy> guard;
  if (EXPECT_TRUE(PF::is_usermode_error(error_code))
      || ret_frame->interrupts_enabled()
      || !Kmem::is_kmem_page_fault(pfa, error_code))
    guard = lock_guard<Lock_guard_inverse_policy>(cpu_lock);

  return handle_page_fault(pfa, error_code, ret_frame->ip(), ret_frame);
}

IMPLEMENT
int
Thread::thread_handle_slow_trap(Trap_state *ts)
{
  Thread *ct = current_thread();
  return ct->handle_slow_trap(ts);
}

PROTECTED inline
int
Thread::handle_slow_trap(Trap_state *ts)
{
  LOG_TRAP;

  if (!ts->user_mode())
    return !Thread::call_nested_trap_handler(ts);

  assert(ts == regs());
  // send exception IPC if requested
  if (send_exception(ts))
    return 1;

  kill();
  return 0;
}

PROTECTED inline
int
Thread::handle_ecall(Return_frame *ret_frame)
{
  Mword state = this->state();
  state_del(Thread_cancel);

  if (state & (Thread_vcpu_user | Thread_alien))
    {
      if (state & Thread_dis_alien)
        {
          state_del_dirty(Thread_dis_alien);
          handle_syscall(ret_frame);
          ret_frame->cause = Return_frame::Ec_l4_alien_after_syscall;
        }

      return handle_slow_trap(static_cast<Trap_state *>(ret_frame));
    }

  handle_syscall(ret_frame);
  return 1;
}

PROTECTED inline
void
Thread::handle_syscall(Return_frame *ret_frame)
{
  // Advance ip to continue execution after ecall instruction
  ret_frame->ip(ret_frame->ip() + 4);

  do_syscall();
}

PROTECTED inline
int
Thread::handle_breakpoint(Return_frame *ret_frame)
{
  if (ret_frame->user_mode())
    return handle_slow_trap(static_cast<Trap_state *>(ret_frame));

  // Breakpoint in kernel
  call_nested_trap_handler(static_cast<Trap_state *>(ret_frame));
  // Advance ip to continue execution after ebreak instruction.
  ret_frame->ip(ret_frame->ip() + Cpu::inst_size(ret_frame->ip()));

  return 1;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && fpu]:

PROTECTED inline
int
Thread::handle_fpu_trap(Return_frame *ret_frame)
{
  // FPU is enabled, so it can't be an FPU trap.
  if (Fpu::is_enabled())
    return handle_slow_trap(static_cast<Trap_state *>(ret_frame));

  // Assume that the illegal instruction exception was caused
  // by a floating point instruction in combination with a disabled FPU.
  // In the case where the exception was actually caused by an illegal
  // instruction, another illegal instruction exception will be thrown
  // when returning from the trap handler. This time the FPU is enabled,
  // therefore we know the cause can't be an FPU trap (see above).
  if (!ret_frame->user_mode())
    panic("FPU trap or illegal instruction exception from inside the kernel!");

  if (!switchin_fpu())
    return handle_slow_trap(static_cast<Trap_state *>(ret_frame));

  return 1;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !fpu]:

PROTECTED inline
int
Thread::handle_fpu_trap(Return_frame *ret_frame)
{
  return handle_slow_trap(static_cast<Trap_state *>(ret_frame));
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

#include "ipi.h"

PROTECTED inline
int
Thread::handle_ipi()
{
  // Clear pending IPI
  Cpu::clear_software_interrupt();

  auto ipis = Ipi::pending_ipis_reset(current_cpu());

  if (ipis & Ipi::Global_request)
    Thread::handle_global_remote_requests_irq(); // must not call schedule()!

  if (ipis & Ipi::Debug)
    {
      Ipi::eoi(Ipi::Debug, current_cpu());
      // Fake trap-state for the nested trap handler and set cause to debug ipi.
      Trap_state ts;
      ts.cause = Trap_state::Ec_l4_debug_ipi;
      Thread::call_nested_trap_handler(&ts);
    }

  if (ipis & Ipi::Request)
    Thread::handle_remote_requests_irq(); // might call schedule()

  return 1;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !mp]:

PROTECTED inline int Thread::handle_ipi() { return 0; }

//----------------------------------------------------------
IMPLEMENTATION [riscv && !cpu_virt]:

IMPLEMENT_OVERRIDE inline
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool)
{
  vcpu_state->version = Vcpu_arch_version;
  vcpu_state->host.saved = false;
}

PRIVATE inline
bool
Thread::handle_hyp_ext_trap(Mword, Mword, Return_frame *)
{ return false; }

//----------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h"]
bool
Thread::arch_ext_vcpu_enabled()
{ return Cpu::has_isa_ext(Cpu::Isa_ext_h); }

PRIVATE
void
Thread::arch_ext_update_guest_space()
{
  if (vcpu_user_space() && nonull_static_cast<Task*>(vcpu_user_space())->is_vm())
    {
      state_add_dirty(Thread_ext_vcpu_has_guest_space);
      regs()->allow_vmm_hyp_load_store(true);
    }
  else
    {
      state_del_dirty(Thread_ext_vcpu_has_guest_space);
      regs()->allow_vmm_hyp_load_store(false);
    }

  // Ensure guest context is activated (so that VMM can use hypervisor
  // load-store instructions) or if guest context has no space (anymore)
  // reset guest context.
  if (this == current())
    switchin_guest_context();
}

IMPLEMENT_OVERRIDE inline
Mword
Thread::arch_check_vcpu_state(bool ext)
{
  if (ext && !check_for_current_cpu())
    return -L4_err::EInval;

  return 0;
}

IMPLEMENT_OVERRIDE
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;
  vcpu_state->host.saved = false;

  if (!ext || (state() & Thread_ext_vcpu_enabled))
    return;

  Hyp_ext_state *v = vm_state(vcpu_state);
  v->hlsi_failed = false;
  v->remote_fence = Hyp_ext_state::Rfnc_none;
  v->remote_fence_hart_mask = 0;
  v->remote_fence_start_addr = 0;
  v->remote_fence_size = 0;
  v->remote_fence_asid = 0;

  arch_ext_update_guest_space();
}

IMPLEMENT_OVERRIDE
void
Thread::arch_vcpu_set_user_space()
{
  assert(this == current());
  if (state(false) & Thread_ext_vcpu_enabled)
    arch_ext_update_guest_space();
}

PRIVATE inline NEEDS["cpu.h", Thread::handle_slow_trap]
bool
Thread::handle_hyp_ext_trap(Mword cause, Mword val, Return_frame *ret_frame)
{
  // The trap comes from virtualization mode.
  if (ret_frame->virt_mode())
    {
      // For traps from VS-mode the return frame is marked as not coming from
      // user mode, which is technically correct but confuses Fiasco, because in
      // several locations Return_frame::user_mode() is used to decide whether
      // it comes from within Fiasco or not. Therefore, we always mark
      // virtualization return frames as user mode frames. Whether the guest was
      // in VS-mode or VU-mode is already represented in hstatus.SPVP.
      ret_frame->user_mode(true);

      // Assume the illegal instruction exception is a FPU trap caused by lazy
      // FPU switching(see Thread::handle_fpu_trap)
      if (cause == Cpu::Exc_illegal_inst && handle_hyp_ext_fpu_trap())
        return true;

      if (cause & Cpu::Msb)
        {
          // Interrupts, not necessarily related to guest execution
          switch(cause)
            {
              case Cpu::Int_virtual_supervisor_software:
              case Cpu::Int_virtual_supervisor_timer:
              case Cpu::Int_virtual_supervisor_external:
              // TODO: Supervisor guest external interrupt?
                break;
              default:
                // Not originating from within the guest.
                return false;
            }
        }

      assert(ret_frame == regs());
      if (!send_exception(static_cast<Trap_state *>(ret_frame)))
        {
          WARN("Hypervisor trap handling failed: Cause=" L4_MWORD_FMT ", Epc=" L4_MWORD_FMT ", Val=" L4_MWORD_FMT "\n",
               cause, ret_frame->ip(), val);
          kdb_ke("Hypervisor trap handling failed!");
          kill();
        }

      return true;
    }
  else if (ret_frame->hstatus & Cpu::Hstatus_gva)
    {
      // If hstatus.SPV is not set and hstatus.GVA is set, the cause
      // of the exception must be a failed hypervisor load store instruction.
      assert(ret_frame->user_mode());
      assert((state() & Thread_ext_vcpu_enabled));

      // HLV, HLVX, or HSV failed
      vm_state(vcpu_state().access())->hlsi_failed = true;
      // Skip instruction
      ret_frame->ip(ret_frame->ip() + 4);

      return true;
    }
  else
    return false;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt && fpu && lazy_fpu]:

PRIVATE inline
bool
Thread::handle_hyp_ext_fpu_trap()
{
  // FPU is enabled, so it can't be an FPU trap.
  if (Fpu::is_enabled())
    return false;

  return switchin_fpu();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt && (!fpu || !lazy_fpu)]:

PRIVATE inline
bool
Thread::handle_hyp_ext_fpu_trap()
{ return false; }

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !debug]:

extern "C" void sys_ipc_wrapper();

PROTECTED static
void
Thread::do_syscall()
{
  sys_ipc_wrapper();
}

//----------------------------------------------------------------------------
INTERFACE [riscv && debug]:

EXTENSION class Thread
{
public:
  typedef void (*Sys_call)(void);

protected:
  static Sys_call sys_call_entry;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && debug]:

#include "vkey.h"

extern "C" void sys_ipc_wrapper();
Thread::Sys_call Thread::sys_call_entry = sys_ipc_wrapper;

PUBLIC static inline
void
Thread::set_sys_call_entry(Sys_call e)
{
  sys_call_entry = e;
}

PROTECTED static
void
Thread::do_syscall()
{
  sys_call_entry();
}
