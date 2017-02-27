IMPLEMENTATION [arm && 32bit]:

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
{ return exception_triggered() ? _exc_cont.ip() : regs()->ip(); }

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

PUBLIC static inline void
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
}

PRIVATE static inline NEEDS[Thread::set_tpidruro]
bool FIASCO_WARN_RESULT
Thread::copy_utcb_to_ts(L4_msg_tag tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  // if the message is too short just skip the whole copy in
  if (EXPECT_FALSE(tag.words() < (sizeof(Trex) / sizeof(Mword))))
    return true;

  Trap_state *ts = (Trap_state*)rcv->_utcb_handler;
  Utcb *snd_utcb = snd->utcb().access();
  Trex const *sregs = reinterpret_cast<Trex const *>(snd_utcb->values);

  if (EXPECT_FALSE(rcv->exception_triggered()))
    {
      // triggered exception pending
      Mem::memcpy_mwords(ts, snd_utcb->values, 15);
      Return_frame rf = access_once(static_cast<Return_frame const *>(&sregs->s));
      rcv->sanitize_user_state(&rf);
      rcv->_exc_cont.set(ts, &rf);
    }
  else
    rcv->copy_and_sanitize_trap_state(ts, &sregs->s);

  if (tag.transfer_fpu() && (rights & L4_fpage::Rights::W()))
    snd->transfer_fpu(rcv);

  // FIXME: this is an old l4linux specific hack, will be replaced/remved
  if ((tag.flags() & 0x8000) && (rights & L4_fpage::Rights::W()))
    rcv->utcb().access()->user[2] = snd_utcb->values[25];

  rcv->set_tpidruro(sregs);

  bool ret = transfer_msg_items(tag, snd, snd_utcb,
                                rcv, rcv->utcb().access(), rights);

  return ret;
}


PRIVATE static inline NEEDS[Thread::save_fpu_state_to_utcb,
                            Thread::store_tpidruro]
bool FIASCO_WARN_RESULT
Thread::copy_ts_to_utcb(L4_msg_tag, Thread *snd, Thread *rcv,
                        L4_fpage::Rights rights)
{
  Trap_state *ts = (Trap_state*)snd->_utcb_handler;

  {
    auto guard = lock_guard(cpu_lock);
    Utcb *rcv_utcb = rcv->utcb().access();
    Trex *rregs = reinterpret_cast<Trex *>(rcv_utcb->values);

    snd->store_tpidruro(rregs);

    Mem::memcpy_mwords(rcv_utcb->values, ts, 15);
    Continuation::User_return_frame *d
      = reinterpret_cast<Continuation::User_return_frame *>((char*)&rcv_utcb->values[15]);

    snd->_exc_cont.get(d, ts);


    if (EXPECT_TRUE(!snd->exception_triggered()))
      {
        rcv_utcb->values[18] = ts->pc;
        rcv_utcb->values[19] = ts->psr;
      }

    if (rcv_utcb->inherit_fpu() && (rights & L4_fpage::Rights::W()))
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

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && fpu && !arm_v8]:

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

IMPLEMENT_OVERRIDE inline
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
IMPLEMENTATION [arm && 32bit && arm_esr_traps]:

PRIVATE inline
void
Thread::handle_svc(Trap_state *ts)
{
  extern void slowtrap_entry(Trap_state *ts) asm ("slowtrap_entry");
  Unsigned32 pc = ts->pc;
  if (!is_syscall_pc(pc))
    {
      slowtrap_entry(ts);
      return;
    }
  ts->pc = get_lr_for_mode(ts);
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
  sys_call_table[(-pc) / 4]();
}

