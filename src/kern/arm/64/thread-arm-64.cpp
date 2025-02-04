IMPLEMENTATION [arm && 64bit]:

#include "fpu.h"
#include "slowtrap_entry.h"

/**
 * Mangle the error code in case of a kernel lib page fault.
 *
 * On 64bit (ARMv8+) this is a nop, there is no kernel lib.
 */
PUBLIC static inline
Mword
Thread::mangle_kernel_lib_page_fault(Mword, Mword error_code)
{ return error_code; }

EXTENSION class Thread
{
private:
  // synchronous traps from kernel / hyp mode
  static void arm_kernel_sync_entry(Trap_state *ts) asm ("arm_kernel_sync_entry");
};

IMPLEMENT inline
Mword
Thread::user_ip() const
{ return regs()->ip(); }

IMPLEMENT inline
void
Thread::user_ip(Mword ip)
{ regs()->ip(ip); }

PUBLIC [[noreturn]] static inline void
Thread::arm_fast_exit(void *sp, void *pc, void *arg)
{
  register void *r0 asm("x0") = arg;
  asm volatile
    ("  mov sp, %[stack_p]    \n"    // set stack pointer to regs structure
     "  br  %[rfe]            \n"
     : :
     [stack_p] "r" (sp),
     [rfe]     "r" (pc),
     "r" (r0));
  __builtin_unreachable();
}


extern "C" void leave_by_vcpu_upcall(Trap_state *ts)
{
  Thread *c = current_thread();
  Vcpu_state *vcpu = c->vcpu_state().access();
  Mem::memcpy_mwords(vcpu->_regs.s.r, ts->r, 31);
  vcpu->_regs.s.usp = ts->usp;
  vcpu->_regs.s.pc = ts->pc;
  vcpu->_regs.s.pstate = ts->pstate;
  c->vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_sp, c->vcpu_state().usr().get());
}

bool is_permission_fault(Mword error)
{
  return (error & 0x3c) == 0xc;
}

IMPLEMENT
void
Thread::arm_kernel_sync_entry(Trap_state *ts)
{
  auto esr = Thread::get_esr();
  ts->esr = esr;

  if (ts->psr & (1UL << 20))
    {
      // Illegal execution state: This could happen in hyp mode if PPSR_EL2
      // contains an invalid mode during ERET (``Illegal return event from
      // AArch64 state'').
      ts->psr &= ~(Mword{Proc::Status_mode_mask} | Mword{Proc::Status_interrupts_mask});
      ts->psr |= Mword{Proc::Status_mode_user} | Mword{Proc::Status_always_mask};
      ts->esr.ec() = 0xe;
      ts->pf_address = 0UL;
      current_thread()->send_exception(ts);
      return;
    }

  switch (esr.ec())
    {
    case 0x21: // instruction abort from kernel mode
      if (is_permission_fault(esr.raw()))
        {
          ts->dump();
          panic("kernel data execution\n");
        }
      ts->pf_address = get_fault_pfa(esr, true, false);
      call_nested_trap_handler(ts);
      break;
    case 0x25: // data abort from kernel mode
      if (EXPECT_FALSE(!handle_cap_area_fault(ts)))
        {
          ts->pf_address = get_fault_pfa(esr, false, false);
          if (is_transient_mpu_fault(ts, esr))
            return;

          if (!PF::is_read_error(esr.raw()) && is_permission_fault(esr.raw()))
            {
              ts->dump();
              panic("kernel code modification\n");
            }
          call_nested_trap_handler(ts);
        }
      break;

    case 0x3c: // BRK
      call_nested_trap_handler(ts);
      ts->pc += 4;
      break;

    default:
      ts->pf_address = get_fault_pfa(esr, false, false);
      call_nested_trap_handler(ts);
      break;
    }
}

PRIVATE inline
void
Thread::do_syscall()
{
  typedef void Syscall(void);
  extern Syscall *sys_call_table[];
  sys_call_table[0]();
}


PRIVATE inline NEEDS["slowtrap_entry.h"]
void
Thread::handle_svc(Trap_state *ts)
{
  Mword state = this->state();
  state_del(Thread_cancel);
  if (state & (Thread_vcpu_user | Thread_alien))
    {
      if (state & Thread_dis_alien)
        {
          state_del_dirty(Thread_dis_alien);
          do_syscall();
          ts->error_code |= 1 << 16; // ts->esr().alien_after_syscall() = 1;
        }
      else
        // Before syscall was executed. Adjust PC to be on SVC/HVC insn so that
        // the instruction can be restarted.
        ts->pc -= Arm_esr(ts->error_code).il() ? 4 : 2;

      slowtrap_entry(ts);
      return;
    }

  do_syscall();
}

PRIVATE [[nodiscard]] static inline NEEDS[Thread::set_tpidruro,
                                          Thread::set_tpidrurw,
                                          "trap_state.h"]
bool
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // only a complete state will be used.
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = static_cast<Trap_state*>(rcv->_utcb_handler);
  Utcb *snd_utcb = snd->utcb().access();

  Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
  // this skips the eret/continuation work already
  rcv->copy_and_sanitize_trap_state(ts, &r->s);
  rcv->set_tpidruro(r);
  rcv->set_tpidrurw(r);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PRIVATE [[nodiscard]] static inline NEEDS[Thread::store_tpidruro,
                                          Thread::store_tpidrurw,
                                          "trap_state.h"]
bool
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = static_cast<Trap_state*>(snd->_utcb_handler);

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *r = reinterpret_cast<Trex *>(rcv_utcb->values);
    r->s = *ts;
    snd->store_tpidruro(r);
    snd->store_tpidrurw(r);

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      snd->transfer_fpu(rcv);

    __asm__ __volatile__ ("" : : "m"(*r));
  }
  return true;
}

PUBLIC static inline bool Thread::is_fsr_exception(Arm_esr) { return false; }
PUBLIC static inline bool Thread::is_debug_exception(Arm_esr) { return false; }
PUBLIC inline void Thread::handle_debug_exception(Trap_state *) {}
PUBLIC static inline bool Thread::is_debug_exception_fsr(Mword) { return false; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && fpu]:

PUBLIC static
bool
Thread::handle_fpu_trap(Trap_state *ts)
{
  if (Fpu::is_enabled())
    {
      ts->esr.ec() = 0; // tag fpu undef insn
    }
  else if (current_thread()->switchin_fpu())
    {
      return true;
    }
  else
    {
      ts->esr.ec() = 0x07;
      ts->esr.cv() = 1;
      ts->esr.cpt_cpnr() = 10;
    }

  return false;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && arm_sve]:

PRIVATE
bool
Thread::alloc_sve_state(bool inherit_simd_state)
{
  // Free FP/SIMD state if any.
  Fpu_alloc::free_state(fpu_state());

  // Allocate SVE state.
  if (!Fpu_alloc::alloc_state(_quota, fpu_state(), Fpu::State_type::Sve))
    return false;

  if (inherit_simd_state)
    Fpu::init_sve_from_simd(fpu_state().get());

  // Restore SVE state.
  Fpu::restore_state(fpu_state().get());
  return true;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && lazy_fpu && arm_sve]:

IMPLEMENT_OVERRIDE
bool
Thread::handle_sve_trap(Trap_state *)
{
  assert(Fpu::has_sve());

  bool inherit_simd_state = fpu_state().valid();

  // If we do not own the FPU, become the FPU owner.
  if (Fpu::fpu.current().owner() != this && !switchin_fpu())
    return false;

  // Thread already has SVE enabled, so this was just a lazy FPU trap.
  if (fpu_state().get()->type() == Fpu::State_type::Sve)
    return true;

  return alloc_sve_state(inherit_simd_state);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !lazy_fpu && arm_sve]:

IMPLEMENT_OVERRIDE
bool
Thread::handle_sve_trap(Trap_state *)
{
  assert(Fpu::has_sve());

  // We should never get an SVE trap if the thread already has SVE enabled.
  assert(!(fpu_state().valid()
           && fpu_state().get()->type() == Fpu::State_type::Sve));

  return alloc_sve_state(fpu_state().valid());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !fpu]:

PUBLIC static
bool
Thread::handle_fpu_trap(Trap_state *)
{
  return false;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !cpu_virt]:

PRIVATE static inline
Arm_esr
Thread::get_esr()
{
  Arm_esr esr;
  asm volatile ("mrs %0, ESR_EL1" : "=r"(esr));
  return esr;
}

PRIVATE static inline
Address
Thread::get_fault_pfa(Arm_esr hsr, bool /*insn_abt*/, bool /*ext_vcpu*/)
{
  if (EXPECT_FALSE(hsr.pf_fnv())) // bit is RES0 on IFSC!=0x10 / DFSC!=0x10
    return ~0UL;

  Address a;
  asm volatile ("mrs %0, FAR_EL1" : "=r"(a));
  return a;
}

PRIVATE static inline
bool
Thread::handle_cap_area_fault(Trap_state *ts)
{
  Address pfa;
  asm volatile ("mrs %0, FAR_EL1" : "=r"(pfa));

  if (EXPECT_FALSE(!Mem_layout::is_caps_area(pfa)))
    return false;

  if (EXPECT_FALSE(!pagein_tcb_request(ts)))
    return false;

  return true;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && cpu_virt]:

PRIVATE static inline
bool
Thread::handle_cap_area_fault(Trap_state *)
{
  return false; // cannot happen in HYP mode
}

PRIVATE static inline
Arm_esr
Thread::get_esr()
{
  Arm_esr esr;
  asm volatile ("mrs %0, ESR_EL2" : "=r"(esr));
  return esr;
}

PRIVATE static inline
Address
Thread::get_fault_pfa(Arm_esr hsr, bool /*insn_abt*/, bool ext_vcpu)
{
  if (EXPECT_FALSE(hsr.pf_fnv())) // bit is RES0 on IFSC!=0x10 / DFSC!=0x10
    return ~0UL;

  Address a;
  asm volatile ("mrs %0, FAR_EL2" : "=r"(a));
  if (EXPECT_TRUE(!ext_vcpu) || !(Proc::sctlr_el1() & Cpu::Sctlr_m))
    return a;

  if (hsr.pf_s1ptw() || (hsr.pf_fsc() < 0xc))
    // stage 1 walk or access flag translation or size fault
    {
      Mword ipa;
      asm ("mrs %0, HPFAR_EL2" : "=r" (ipa));
      return (ipa << 8) | (a & 0xfff);
    }

  Mword par_tmp, res;
  asm ("mrs %[tmp], PAR_EL1 \n"
       "at  s1e1r, %[va]    \n"
       "isb                 \n"
       "mrs %[res], PAR_EL1 \n"
       "msr PAR_EL1, %[tmp] \n"
       : [tmp] "=&r" (par_tmp),
         [res] "=r" (res)
       : [va]  "r" (a));

  if (res & 1)
    return ~0UL;

  return (res & 0x00fffffffffff000) | (a & 0xfff);
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && virt_obj_space]:

IMPLEMENT_OVERRIDE inline
bool
Thread::pagein_tcb_request(Return_frame *regs)
{
  // Counterpart: Mem_layout::read_special_safe()
  assert (!regs->esr.pf_write()); // must be a read
  assert (regs->esr.il());        // must be a 32bit wide insn
  // we assume the instruction is a ldr with the target register
  // in the lower 5 bits
  unsigned rt = *reinterpret_cast<Mword*>(regs->pc) & 0x1f;

  // skip faulting instruction
  regs->pc += 4;
  // tell program that a pagefault occurred we cannot handle
  regs->psr |= 0x40000000;	// set zero flag in psr
  regs->r[rt] = 0;

  return true;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && mpu]:

#include "kmem.h"

/**
 * Check for transient MPU fault caused by optimized entry code.
 *
 * The entry code manipulates the kernel heap-, kip- and ku_mem-MPU-regions.
 * Legally, an ISB instruction would be required but this is practically not
 * needed and would impose an undue performance penalty. Instead, handle such
 * data aborts gracefully in case they ever happen.
 */
PRIVATE static inline NEEDS["kmem.h"]
bool
Thread::is_transient_mpu_fault(Trap_state *ts, Arm_esr esr)
{
  switch (esr.pf_fsc())
    {
    case 0b000100:  // level 0 translation fault
      // Only kernel heap region start/end is adapted on entry
      if (!(*Kmem::kdir)[Kpdir::Kernel_heap].contains(ts->pf_address))
        return false;
      break;
    case 0b001100:  // level 0 permission fault
      // Only the KIP permissions are manipulated.
      if (!(*Kmem::kdir)[Kpdir::Kip].contains(ts->pf_address))
        return false;
      break;
    default:
      return false;
    }

  Mem::isb();

  static bool has_warned = false;
  if (!has_warned)
    {
      has_warned = true;
      WARN("Unexpected in-kernel data abort. Add an ISB to entry path?\n");
    }

  return true;
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && !mpu]:

PRIVATE static inline
bool
Thread::is_transient_mpu_fault(Trap_state *, Arm_esr)
{ return false; }
