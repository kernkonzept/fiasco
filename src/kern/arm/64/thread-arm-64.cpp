IMPLEMENTATION [arm && 64bit]:

#include "fpu.h"

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

IMPLEMENT inline
bool
Thread::pagein_tcb_request(Return_frame *regs)
{
  assert (!regs->esr.pf_write()); // must be a read
  assert (regs->esr.il());        // must be a 32bit wide insn
  // we assume the instruction is a ldr with the target register
  // in the lower 5 bits
  unsigned rt = *(Mword*)regs->pc & 0x1f;

  // skip faulting instruction
  regs->pc += 4;
  // tell program that a pagefault occurred we cannot handle
  regs->psr |= 0x40000000;	// set zero flag in psr
  regs->r[rt] = 0;

  return true;
}

PUBLIC static inline void
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
}


extern "C" void leave_by_vcpu_upcall(Trap_state *ts)
{
  Thread *c = current_thread();
  Vcpu_state *vcpu = c->vcpu_state().access();
  Mem::memcpy_mwords(vcpu->_regs.s.r, ts->r, 31);
  vcpu->_regs.s.usp = ts->usp;
  vcpu->_regs.s.pc = ts->pc;
  vcpu->_regs.s.pstate = ts->pstate;
  c->fast_return_to_user(vcpu->_entry_ip, vcpu->_sp, c->vcpu_state().usr().get());
}

IMPLEMENT
void
Thread::arm_kernel_sync_entry(Trap_state *ts)
{
  auto esr = Thread::get_esr();
  ts->esr = esr;

  // test for illegal execution state bit in PSR
  if (ts->psr & (1UL << 20))
    {
      ts->psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
      ts->psr |= Proc::Status_mode_user | Proc::Status_always_mask;
      ts->esr.ec() = 0xe;
      current_thread()->send_exception(ts);
      return;
    }

  switch (esr.ec())
    {
    case 0x25: // data abort from kernel mode
      if (EXPECT_FALSE(!handle_cap_area_fault(ts)))
        call_nested_trap_handler(ts);
      break;

    case 0x3c: // BRK
      call_nested_trap_handler(ts);
      ts->pc += 4;
      break;

    default:
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

PRIVATE inline
void
Thread::handle_svc(Trap_state *ts)
{
  extern void slowtrap_entry(Trap_state *ts) asm ("slowtrap_entry");
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
        ts->pc -= 2 << ts->esr.il();

      slowtrap_entry(ts);
      return;
    }

  do_syscall();
}

PRIVATE static inline
bool
Thread::is_syscall_pc(Address)
{
  return false; //Address(-0x0c) <= pc && pc <= Address(-0x08);
}

PRIVATE static inline
Mword
Thread::get_lr_for_mode(Return_frame const *rf)
{
  return rf->r[30];
}

PRIVATE static inline
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag const &tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // only a complete state will be used.
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
  Utcb *snd_utcb = snd->utcb().access();

  Trex const *r = reinterpret_cast<Trex const *>(snd_utcb->values);
  // this skips the eret/continuation work already
  ts->copy_and_sanitize(&r->s);
  rcv->set_tpidruro(r);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::W()))
    snd->transfer_fpu(rcv);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}

PRIVATE static inline NEEDS[Thread::save_fpu_state_to_utcb,
                            Thread::store_tpidruro,
                            "trap_state.h"]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag const &, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *r = reinterpret_cast<Trex *>(rcv_utcb->values);
    r->s = *ts;
    snd->store_tpidruro(r);

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::W()))
      snd->transfer_fpu(rcv);

    __asm__ __volatile__ ("" : : "m"(*r));
  }
  return true;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && fpu]:

PUBLIC static
bool
Thread::handle_fpu_trap(Trap_state *ts)
{
  if (Fpu::is_enabled())
    {
      assert(Fpu::fpu.current().owner() == current());
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
Thread::get_fault_pfa(Arm_esr /*hsr*/, bool /*insn_abt*/, bool /*ext_vcpu*/)
{
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
       "mrs %[res], PAR_EL1 \n"
       "msr PAR_EL1, %[tmp] \n"
       : [tmp] "=&r" (par_tmp),
         [res] "=r" (res)
       : [va]  "r" (a));

  if (res & 1)
    return ~0UL;

  return (res & 0x00fffffffffff000) | (a & 0xfff);
}

PRIVATE inline
void
Arm_vtimer_ppi::mask()
{
  Mword v;
  asm volatile("mrs %0, cntv_ctl_el0\n"
               "orr %0, %0, #0x2              \n"
               "msr cntv_ctl_el0, %0\n" : "=r" (v));
}

