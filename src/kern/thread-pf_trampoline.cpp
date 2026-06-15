INTERFACE [pagefault_trampoline]:

#include "pf_trampoline.h"

EXTENSION class Thread
{
protected:
  /**
   * Handle page fault in thread trampoline.
   *
   * In context of the kernel page fault handler, set up the stack frame that
   * the thread resumes execution at the thread trampoline page fault handler
   * with the required information.
   */
  void arch_return_to_pf_trampoline(Address pfa, Mword pf_info, Trap_state *ts,
                                    Pf_trampoline *tramp_state,
                                    User_ptr<Pf_trampoline> tramp_state_usr);

  /**
   * Setup trap state from page fault trampoline state and trigger exception
   * before returning to userland.
   *
   * \param ts           Trap_state to be populated.
   * \param tramp_state  Pf_trampoline state.
   * \param artificial_exception  If false, reflect the PF to the exception
   *                              handler. If true, trigger an artificial
   *                              exception at the exception  handler.
   *
   * Either the original PF exception, as recorded in `tramp_state`, could not
   * be handled at the trampoline page fault handler and should be reflected to
   * the exception handler of the thread (artificial=false). Or someone else
   * registered an exception at this thread while this thread executed a PF
   * trampoline handler during which it is not allowed to be interrupted
   * (artificial=true).
   *
   * Copy and sanitize `Trap_state` originating from `tramp_state` so that the
   * exception IPC can be synthesized.
   *
   * Called from Thread_object::sys_vcpu_control().
   *
   * \pre No exception pending.
   */
  [[noreturn]]
  void arch_exception_from_pf_trampoline(Trap_state *ts,
                                         Pf_trampoline const *tramp_state,
                                         bool artificial_exception);

  /**
   * Return to userland after the PF was handled at the trampoline PF handler.
   *
   * \param tramp_state  Pf_trampoline state.
   *
   * Called from Thread_object::sys_vcpu_control().
   *
   * \pre No exception pending.
   */
  [[noreturn]]
  void arch_return_from_pf_trampoline(Pf_trampoline const *tramp_state);

  Ku_mem_ptr<Pf_trampoline> _pf_tramp_state;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [pagefault_trampoline]:

#include "feature.h"

KIP_KERNEL_FEATURE("pf_tramp");

PUBLIC inline
Thread::Ku_mem_ptr<Pf_trampoline> const &
Thread::pf_tramp_state() const
{ return _pf_tramp_state; }
