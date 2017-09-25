INTERFACE:
EXTENSION class Thread
{
protected:
  void arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext);
};

IMPLEMENTATION:

#include "logdefs.h"
#include "task.h"
#include "vcpu.h"

IMPLEMENT_DEFAULT inline
void Thread::arch_init_vcpu_state(Vcpu_state *vcpu, bool /*ext*/)
{
  vcpu->version = Vcpu_arch_version;
}


PUBLIC inline NEEDS["task.h"]
void
Thread::vcpu_set_user_space(Task *t)
{
  assert (current() == this);
  if (t)
    t->inc_ref();

  Task *old = static_cast<Task*>(_space.vcpu_user());
  _space.vcpu_user(t);

  if (old)
    {
      if (!old->dec_ref())
	{
	  rcu_wait();
	  delete old;
	}
    }
}

PUBLIC inline NEEDS["logdefs.h", "vcpu.h"]
bool
Thread::vcpu_pagefault(Address pfa, Mword err, Mword ip)
{
  (void)ip;
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


