IMPLEMENTATION [riscv]:

PRIVATE inline
bool
Task::invoke_arch(L4_msg_tag &, Utcb *)
{
  return false;
}

extern "C" void vcpu_resume(Trap_state *, Return_frame *sp)
   FIASCO_FASTCALL FIASCO_NORETURN;

IMPLEMENT_OVERRIDE
int
Task::resume_vcpu(Context *ctxt, Vcpu_state *vcpu, bool user_mode)
{
  if (user_mode)
    {
      ctxt->vcpu_save_host_regs(vcpu);
      ctxt->state_add_dirty(Thread_vcpu_user);
      vcpu->state |= Vcpu_state::F_traps;
    }
  else
    // When resuming to vCPU host mode populate the host mode preserved
    // registers in the vCPU state with their current values.
    ctxt->vcpu_copy_host_regs(vcpu);

  Trap_state ts;
  ctxt->copy_and_sanitize_trap_state(&ts, &vcpu->_regs.s);

  ctxt->space_ref()->user_mode(user_mode);
  switchin_context(ctxt->space());
  vcpu_resume(&ts, ctxt->regs());
}
