INTERFACE:
EXTENSION class Thread
{
protected:
  /**
   * Check that everything is ready to initialize the vCPU state.
   *
   * Run any architecture-specific preparation steps and checks to make sure
   * that the thread vCPU state can be initialized and enabled.
   *
   * If this method indicates success, then the caller is expected to
   * initialize and enable the vCPU state.
   *
   * \param[in] ext  Indicate whether the extended vCPU state is about to be
   *                 initialized and enabled.
   *
   * \return Status of the preparation steps and checks. The value 0 indicates
   *         success, any other value indicates a specific error.
   */
  Mword arch_check_vcpu_state(bool ext);

  /**
   * Initialize the vCPU state.
   *
   * Architecture-specific vCPU state initialization. This method is called
   * after the successful call to \ref arch_check_vcpu_state and after the
   * \ref _vcpu_state member has been set to the vCPU state, but before the
   * vCPU state is finally enabled.
   *
   * This method is not expected to fail.
   *
   * \param[in,out] vcpu_state  vCPU state to initialize.
   * \param[in]     ext         Indicate whether the extended vCPU state needs
   *                            to be initialized.
   */
  void arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext);
  void arch_vcpu_set_user_space();
};

IMPLEMENTATION:

#include "logdefs.h"
#include "task.h"
#include "vcpu.h"

IMPLEMENT_DEFAULT inline
Mword Thread::arch_check_vcpu_state(bool)
{ return 0; }

IMPLEMENT_DEFAULT inline
void Thread::arch_init_vcpu_state(Vcpu_state *vcpu, bool /*ext*/)
{
  vcpu->version = Vcpu_arch_version;
}

IMPLEMENT_DEFAULT inline
void Thread::arch_vcpu_set_user_space()
{}

PUBLIC inline NEEDS["task.h"]
void
Thread::vcpu_set_user_space(Task *t)
{
  assert (current() == this);
  if (t)
    t->inc_ref();

  Task *old = static_cast<Task*>(_space.vcpu_user());
  _space.vcpu_user(t);

  arch_vcpu_set_user_space();

  if (old && !old->dec_ref())
    delete old;
}

PUBLIC inline NEEDS["logdefs.h", "vcpu.h"]
bool
Thread::vcpu_pagefault(Address pfa, Mword err, Mword ip)
{
  static_cast<void>(ip);
  Vcpu_state *vcpu = vcpu_state().access();
  if (vcpu_pagefaults_enabled(vcpu))
    {
      spill_user_state();
      vcpu_enter_kernel_mode(vcpu);
      LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
	  l->type = 3;
	  l->state = vcpu->_saved_state;
	  l->ip = ip;
	  l->sp = pfa;
          l->err = err;
	  l->space = vcpu_user_space() ? static_cast<Task*>(vcpu_user_space())->dbg_id() : ~0;
	  );
      vcpu->_regs.s.set_pagefault(pfa, err);
      vcpu_save_state_and_upcall();
      return true;
    }

  return false;
}

/**
 * Check for pending IPCs/IRQs after handling a page fault or exception IPC,
 * as they disable asynchronous vCPU IPC reception temporarily.
 *
 * \pre cpu_lock.test() == true
 */
PUBLIC inline NEEDS["vcpu.h", "lock_guard.h", "cpu_lock.h",
                    Thread::exception_triggered]
void
Thread::vcpu_upcall_pending_ipc()
{
  // See Thread_object::sys_vcpu_resume()
  if (!(state() & Thread_vcpu_enabled)) [[likely]]
    return;

  assert(cpu_lock.test());

  Vcpu_state *vcpu = vcpu_state().access();
  if (!(vcpu->state & Vcpu_state::F_irqs) || sender_list()->empty())
    return;

  if (exception_triggered())
    return;

  spill_user_state();
  if (vcpu_enter_kernel_mode(vcpu))
    vcpu = vcpu_state().access();

  do_ipc(L4_msg_tag(), 0, nullptr, true, nullptr,
         L4_timeout_pair(L4_timeout::Zero, L4_timeout::Zero),
         &vcpu->_ipc_regs, L4_fpage::Rights::FULL());

  vcpu = vcpu_state().access(true);

  LOG_TRACE("VCPU events", "vcpu", this, Vcpu_log,
      l->type = 5;
      l->state = vcpu->_saved_state;
      l->ip = regs()->ip();
      l->sp = regs()->sp();
      l->space = vcpu_user_space() ? static_cast<Task*>(vcpu_user_space())->dbg_id() : ~0;
      );

  vcpu->_regs.set_ipc_upcall();
  vcpu_save_state_and_upcall();
}
