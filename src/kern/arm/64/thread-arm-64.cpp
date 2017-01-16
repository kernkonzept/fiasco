IMPLEMENTATION [arm && 64bit]:

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
  //if ((*(Mword*)regs->pc & 0xfff00fff ) == 0xe5900000)
  if (*(Mword*)regs->pc == 0xe59ee000)
    {
      // printf("TCBR: %08lx\n", *(Mword*)regs->pc);
      // skip faulting instruction
      regs->pc += 4;
      // tell program that a pagefault occurred we cannot handle
      regs->psr |= 0x40000000;	// set zero flag in psr
      regs->r[0] = 0;

      return true;
    }
  return false;
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

extern "C" void leave_by_vcpu_upcall(Trap_state *ts)
{
  Thread *c = current_thread();
  Vcpu_state *vcpu = c->vcpu_state().access();
  vcpu->_regs.s = *ts;
  c->fast_return_to_user(vcpu->_entry_ip, vcpu->_entry_sp, c->vcpu_state().usr().get());
}

IMPLEMENT
void
Thread::arm_kernel_sync_entry(Trap_state *ts)
{
  auto esr = Thread::get_esr();
  ts->esr = esr;
  switch (esr.ec())
    {
    case 0x25: // data abort -> handle cap space access from the kernel
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
Thread::handle_svc(Trap_state *ts)
{
  extern void slowtrap_entry(Trap_state *ts) asm ("slowtrap_entry");
  Mword state = this->state();
  state_del(Thread_cancel);
  if (state & (Thread_vcpu_user | Thread_alien))
    {
      if (state & Thread_dis_alien)
        state_del_dirty(Thread_dis_alien);
      else
        {
          slowtrap_entry(ts);
          return;
        }
    }

  typedef void Syscall(void);
  extern Syscall *sys_call_table[];
  sys_call_table[0]();
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
Thread::is_syscall_pc(Address)
{
  return true; //Address(-0x0c) <= pc && pc <= Address(-0x08);
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

