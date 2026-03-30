IMPLEMENTATION [arm && pagefault_trampoline]:

IMPLEMENT
void
Thread::arch_return_to_pf_trampoline(Address pfa, Mword pf_info, Trap_state *ts,
                                     Pf_trampoline *tramp_state,
                                     User_ptr<Pf_trampoline> tramp_state_usr)
{
  assert (current() == this);

  ts->r[0] = reinterpret_cast<Mword>(tramp_state_usr.get());
  ts->r[1] = PF::addr_to_msgword0(pfa, pf_info);

  tramp_state->ts.pf_address = pfa;
}

IMPLEMENT
void
Thread::arch_return_from_pf_trampoline(Pf_trampoline const *tramp_state)
{
  auto *r = regs();

  // Basically the same as Task::resume_vcpu() but without the task switching.
  // No exceptions must be pending -- they wouldn't be handled properly!
  //
  // We exit the kernel via a different path (this function does not return),
  // passing a Trap_state that replaces the Entry_frame created upon entering
  // the kernel via a syscall.
  static_assert(sizeof(Entry_frame) >= sizeof(Trap_state));
  Trap_state *ts = reinterpret_cast<Trap_state *>(r + 1) - 1;
  copy_and_sanitize_trap_state(ts, &tramp_state->ts);

  resume_normal_after_pf_trampoline(ts, r);
}

IMPLEMENT
void
Thread::arch_exception_from_pf_trampoline(Trap_state *ts,
                                          Pf_trampoline const *tramp_state,
                                          bool artificial_exception)
{
  assert(cpu_lock.test());

  copy_and_sanitize_trap_state(ts, &tramp_state->ts);
  if (artificial_exception)
    {
      ts->error_code = 0x3e << 26;
      ts->pf_address = 0;
    }
  else
    {
      ts->error_code = tramp_state->ts.error_code;
      ts->pf_address = tramp_state->ts.pf_address;
    }

  ts->disable_continuation(); // not initialized by copy_and_sanitize_trap_state()

  resume_trigger_exception_after_pf_trampoline(ts);
}
