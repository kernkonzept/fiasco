IMPLEMENTATION [riscv && pagefault_trampoline]:

IMPLEMENT
void
Thread::arch_return_to_pf_trampoline(Address pfa, Mword pf_info, Trap_state *ts,
                                     Pf_trampoline *tramp_state,
                                     User_ptr<Pf_trampoline> tramp_state_usr)
{
  assert (current() == this);

  ts->a0 = reinterpret_cast<Mword>(tramp_state_usr.get());
  ts->a1 = PF::addr_to_msgword0(pfa, pf_info);

  tramp_state->ts.tval = pfa;
}

IMPLEMENT
void
Thread::arch_return_from_pf_trampoline(Pf_trampoline const *tramp_state)
{
  // Basically the same as Task::resume_vcpu() but without the task switching.
  // No exceptions must be pending -- they wouldn't be handled properly!
  Trap_state ts;
  copy_and_sanitize_trap_state(&ts, &tramp_state->ts);

  resume_normal_after_pf_trampoline(&ts, regs());
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
      ts->status = 0;
      ts->cause = Trap_state::Ec_l4_exregs_exception;
      ts->tval = 0;
    }
  else
    {
      ts->status = tramp_state->ts.status;
      ts->cause = tramp_state->ts.cause;
      ts->tval = tramp_state->ts.tval;
    }

  ts->disable_continuation(); // not initialized by copy_and_sanitize_trap_state()

  resume_trigger_exception_after_pf_trampoline(ts);
}
