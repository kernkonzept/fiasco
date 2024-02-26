IMPLEMENTATION [arm && 32bit]:

#include "mem_op.h"

/**
 * Mangle the error code in case of a kernel lib page fault.
 *
 * All page faults caused by code on the kernel lib page are
 * write page faults, because the code implements atomic
 * read-modify-write.
 */
PUBLIC static inline
Mword
Thread::mangle_kernel_lib_page_fault(Mword pc, Mword error_code)
{
  if (EXPECT_FALSE((pc & Kmem::Kern_lib_base) == Kmem::Kern_lib_base))
    return error_code | (1UL << 6);

  return error_code;
}

IMPLEMENT inline NEEDS[Thread::exception_triggered]
Mword
Thread::user_ip() const
{
  Mword ret;

  if (exception_triggered())
    ret = _exc_cont.ip();
  else
    {
      ret = regs()->ip();
      if (regs()->psr & Proc::Status_thumb)
        ret |= 1U;
    }

  return ret;
}

IMPLEMENT inline NEEDS[Thread::exception_triggered]
void
Thread::user_ip(Mword ip)
{
  if (exception_triggered())
    _exc_cont.ip(ip);
  else
    {
      Entry_frame *r = regs();
      r->ip(ip & ~1UL);
      if (ip & 1U)
        r->psr |= Proc::Status_thumb;
      else
        r->psr &= ~static_cast<Mword>(Proc::Status_thumb);
    }
}

PUBLIC static inline void FIASCO_NORETURN
Thread::arm_fast_exit(void *sp, void *pc, void *arg)
{
  register void *r0 asm("r0") = arg;
  asm volatile
    ("  mov sp, %[stack_p]    \n"    // set stack pointer to regs structure
     "  mov pc, %[rfe]        \n"
     : :
     [stack_p] "r" (sp),
     [rfe]     "r" (pc),
     "r" (r0));
  __builtin_unreachable();
}

PRIVATE static inline NEEDS[Thread::set_tpidruro]
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // if the message is too short just skip the whole copy in
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = static_cast<Trap_state*>(rcv->_utcb_handler);
  Utcb *snd_utcb = snd->utcb().access();
  Trex const *sregs = reinterpret_cast<Trex const *>(snd_utcb->values);

  if (EXPECT_FALSE(rcv->exception_triggered()))
    {
      // triggered exception pending -- copy pf_address, esr, r0..r12
      Mem::memcpy_mwords(ts, snd_utcb->values, 15);
      Return_frame rf = access_once(static_cast<Return_frame const *>(&sregs->s));
      rcv->sanitize_user_state(&rf);
      rcv->_exc_cont.set(ts, &rf);
    }
  else
    rcv->copy_and_sanitize_trap_state(ts, &sregs->s);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::CS()))
    snd->transfer_fpu(rcv);

  // FIXME: this is an old l4linux specific hack, will be replaced/remved
  if ((tag.flags() & 0x8000) && (rights & L4_fpage::Rights::CS()))
    rcv->utcb().access()->user[2] = snd_utcb->values[25];

  rcv->set_tpidruro(sregs);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}


PRIVATE static inline NEEDS[Thread::store_tpidruro]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = static_cast<Trap_state*>(snd->_utcb_handler);

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *rregs = reinterpret_cast<Trex *>(rcv_utcb->values);

    snd->store_tpidruro(rregs);

    // copy pf_address, esr, r0..r12
    Mem::memcpy_mwords(rcv_utcb->values, ts, 15);
    Continuation::User_return_frame *d
      = reinterpret_cast<Continuation::User_return_frame *>(
          &rcv_utcb->values[15]);

    snd->_exc_cont.get(d, ts);


    if (EXPECT_TRUE(!snd->exception_triggered()))
      {
        rcv_utcb->values[18] = ts->pc;
        rcv_utcb->values[19] = ts->psr;
      }

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::CS()))
      {
        snd->save_fpu_state_to_utcb(ts, rcv_utcb);
        snd->transfer_fpu(rcv);
      }
  }
  return true;
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

PUBLIC static inline
bool
Thread::is_fsr_exception(Arm_esr esr)
{
  enum
  {
    Hsr_ec_prefetch_abort_lower = 0x20,
    Hsr_ec_prefetch_abort       = 0x21,
    Hsr_ec_data_abort_lower     = 0x24,
    Hsr_ec_data_abort           = 0x25,
  };

  return    esr.ec() == Hsr_ec_prefetch_abort_lower
         || esr.ec() == Hsr_ec_prefetch_abort
         || esr.ec() == Hsr_ec_data_abort_lower
         || esr.ec() == Hsr_ec_data_abort;
}

PUBLIC static inline
bool
Thread::is_debug_exception(Arm_esr esr)
{
  enum
  {
    Hsr_fsc_debug               = 0x22,
  };

  // LPAE type or non-LPAE converted to LPAE by map_fsr_user.
  return is_fsr_exception(esr) && esr.pf_fsc() == Hsr_fsc_debug;
}

PUBLIC inline NEEDS[Thread::call_nested_trap_handler]
void
Thread::handle_debug_exception(Trap_state *ts)
{
  if (PF::is_usermode_error(ts->error_code))
    {
      // Convert DBGDSCR.MOE to corresponding AArch64 Trap_state::esr syndromes
      // so that it is accessible to user space.
      Mword v;
      asm volatile("mrc p14, 0, %0, c0, c1, 0" : "=r" (v)); // DBGDSCR
      Mword moe = (v >> 2) & 0xf;
      Arm_esr esr = ts->esr;
      switch (moe)
        {
        case 1: // Breakpoint debug event
          ts->esr = Arm_esr::make_breakpoint();
          break;

        case 2: // Asynchronous watchpoint debug event
        case 10: // Synchronous watchpoint debug event
          ts->esr = Arm_esr::make_watchpoint(esr.pf_cache_maint(),
                                             esr.pf_write());
          break;

        case 3: // BKPT instruction debug event
          // Unfortunaltely the immediate of the bkpt instruction is not
          // available. We always report it as zero.
          ts->esr = Arm_esr::make_bkpt_insn(esr.il());
          break;

        case 5: // Vector catch debug event
          ts->esr = Arm_esr::make_vector_catch_aarch32();
          break;

        default:
          // Reserved or should only be triggered by an external debugger.
          ts->esr = esr;
          call_nested_trap_handler(ts);
          return;
        }

      if (send_exception(ts))
        return;

      // Restore original esr so that JDB sees the truth.
      ts->esr = esr;
    }

  // Debug exception from within the kernel or if exception IPC failed.
  call_nested_trap_handler(ts);
}

IMPLEMENT_OVERRIDE inline NEEDS["mem_op.h"]
bool
Thread::check_and_handle_linux_cache_api(Trap_state *ts)
{
  if (ts->esr.ec() == 0x11 && ts->r[7] == 0xf0002)
    {
      if (ts->r[2] == 0)
        Mem_op::arm_mem_cache_maint(Mem_op::Op_cache_coherent,
                                    reinterpret_cast<void *>(ts->r[0]),
                                    reinterpret_cast<void *>(ts->r[1]));
      ts->r[0] = 0;
      return true;
    }
  else
    return false;
}

IMPLEMENT_OVERRIDE inline
bool
Thread::check_and_handle_mem_op_fault(Mword error_code, Return_frame *ret_frame)
{
  // cache operations we carry out for user space might cause PFs, we just
  // ignore those
  if (EXPECT_FALSE(!PF::is_usermode_error(error_code))
      && EXPECT_FALSE(is_ignore_mem_op_in_progress()))
    {
      set_kernel_mem_op_hit();
      ret_frame->pc += 4;
      return true;
    }
  else
    return false;
}

extern "C"
Vcpu_state *
current_prepare_vcpu_return_to_kernel(Thread *c, Vcpu_state *vcpu)
{
  c->prepare_vcpu_return_to_kernel(vcpu->_entry_ip, vcpu->_sp);
  return c->vcpu_state().usr().get();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && !arm_lpae]:

PUBLIC static inline
bool
Thread::is_debug_exception_fsr(Mword error_code)
{
  enum
  {
    Fsr_fs_mask  = 0x40f,
    Fsr_fs_debug = 0x2,
  };

  return (error_code & Fsr_fs_mask) == Fsr_fs_debug;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && arm_lpae]:

PUBLIC static inline
bool
Thread::is_debug_exception_fsr(Mword error_code)
{
  enum
  {
    Fsr_status_mask  = 0x3f,
    Fsr_status_debug = 0x22,
  };

  return (error_code & Fsr_status_mask) == Fsr_status_debug;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && fpu]:

IMPLEMENT_OVERRIDE inline
bool
Thread::check_and_handle_coproc_faults(Trap_state *ts)
{
  if (!ts->exception_is_undef_insn())
    return false;

  Unsigned32 opcode;

  if (ts->psr & Proc::Status_thumb)
    {
      Unsigned16 v =
        Thread::peek_user(reinterpret_cast<Unsigned16 *>(ts->pc), this);

      if (EXPECT_FALSE(is_kernel_mem_op_hit_and_clear()))
        return true;

      if ((v >> 11) <= 0x1c)
        return false;

      opcode =
        (v << 16)
        | Thread::peek_user(reinterpret_cast<Unsigned16 *>(ts->pc + 2), this);
    }
  else
    opcode = Thread::peek_user(reinterpret_cast<Unsigned32 *>(ts->pc), this);

  if (EXPECT_FALSE(is_kernel_mem_op_hit_and_clear()))
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
      ts->pc += 4;
      return true;
    }

  if (Fpu::is_enabled())
    {
      if (Fpu::is_emu_insn(opcode))
        return Fpu::emulate_insns(opcode, ts);

      ts->esr.ec() = 0; // tag fpu undef insn
    }
  else if (current_thread()->switchin_fpu())
    {
      if (Fpu::is_emu_insn(opcode))
        return Fpu::emulate_insns(opcode, ts);
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
IMPLEMENTATION [arm && 32bit && arm_esr_traps]:

PRIVATE inline
void
Thread::do_syscall(Unsigned32 r5)
{
  typedef void Syscall(void);
  extern Syscall *sys_call_table[];
  sys_call_table[r5]();
}

PRIVATE inline
void
Thread::handle_svc(Trap_state *ts)
{
  Unsigned32 r5 = ts->r[5];
  if (EXPECT_FALSE(r5 > 1))
    {
      // Adjust PC to point to trapped bogus syscall instruction.
      ts->pc -= Arm_esr(ts->error_code).il() ? 4 : 2;
      slowtrap_entry(ts);
      return;
    }

  Mword state = this->state();
  state_del(Thread_cancel);
  if (state & (Thread_vcpu_user | Thread_alien))
    {
      if (state & Thread_dis_alien)
        {
          state_del_dirty(Thread_dis_alien);
          do_syscall(r5);
          ts->error_code |= 0x40; // see ivt.S alien_syscall
        }
      else
        // Adjust PC to be on SVC/HVC insn so that the instruction can either
        // be restarted (alien thread before syscall) or can be examined in
        // vCPU entry handler.
        ts->pc -= Arm_esr(ts->error_code).il() ? 4 : 2;

      slowtrap_entry(ts);
      return;
    }

  do_syscall(r5);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && virt_obj_space]:

IMPLEMENT_OVERRIDE inline
bool
Thread::pagein_tcb_request(Return_frame *regs)
{
  // Counterpart: Mem_layout::read_special_safe()
  if (*reinterpret_cast<Mword*>(regs->pc) == 0xe59ee000) // ldr lr,[lr]
    {
      // skip faulting instruction
      regs->pc += 4;
      // tell program that a pagefault occurred we cannot handle
      regs->psr |= 0x40000000; // set zero flag in psr
      regs->km_lr = 0;

      return true;
    }
  return false;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && !cpu_virt]:

PUBLIC static inline template<typename T>
T Thread::peek_user(T const *adr, Context *c)
{
  T v;
  c->set_ignore_mem_op_in_progress(true);
  v = *adr;
  c->set_ignore_mem_op_in_progress(false);
  return v;
}
