INTERFACE [arm]:

EXTENSION class Context
{
protected:
  void sanitize_user_state(Return_frame *dst) const;
};

// ------------------------------------------------------------------------
INTERFACE [arm_v6plus]:

EXTENSION class Context
{
protected:
  Mword _tpidrurw;
  Mword _tpidruro;
};

// ------------------------------------------------------------------------
INTERFACE [arm_v8plus && bit64]:

EXTENSION class Context
{
protected:
  Mword _tpidr2rw;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"		// current()
#include "l4_types.h"
#include "cpu_lock.h"
#include "kmem.h"
#include "lock_guard.h"
#include "space.h"
#include "thread_state.h"
#include "utcb_support.h"

IMPLEMENT inline NEEDS[Context::spill_user_state, Context::store_tpidrurw,
                       Context::load_tpidrurw, Context::load_tpidruro,
                       Context::arm_switch_gp_regs]
void
Context::switch_cpu(Context *t)
{
  spill_user_state();
  store_tpidrurw();
  switch_vm_state(t);
  t->fill_user_state();
  t->load_tpidrurw();
  t->load_tpidruro();
  arm_switch_gp_regs(t);
}

/** Thread context switchin.  Called on every re-activation of a
    thread (switch_exec()).  This method is public only because it is
    called by an ``extern "C"'' function that is called
    from assembly code (call_switchin_context).
 */
IMPLEMENT
void Context::switchin_context(Context *from)
{
  assert (this == current());
  assert (state() & Thread_ready_mask);
  from->handle_lock_holder_preemption();

  // switch to our page directory if necessary
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

  Utcb_support::current(this->utcb().usr());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  dst->psr &= ~(Mword{Proc::Status_mode_mask} | Mword{Proc::Status_interrupts_mask});
  dst->psr |= Mword{Proc::Status_mode_user} | Mword{Proc::Status_always_mask};
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_v6plus]:

PROTECTED inline void Context::arch_setup_utcb_ptr()
{
  _tpidrurw = _tpidruro = reinterpret_cast<Mword>(utcb().usr().get());
}

IMPLEMENT_OVERRIDE inline
void
Context::arch_update_vcpu_state(Vcpu_state *vcpu)
{
  vcpu->host.tpidruro = _tpidruro;
}

PUBLIC inline
Mword
Context::tpidrurw() const
{
  return _tpidrurw;
}

PUBLIC inline
Mword
Context::tpidruro() const
{
  return _tpidruro;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm_v6plus && !cpu_virt]:

IMPLEMENT_OVERRIDE inline
void
Context::arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load)
{
  _tpidruro = vcpu->host.tpidruro;
  if (do_load)
    load_tpidruro();
}

IMPLEMENT_OVERRIDE inline
void
Context::arch_load_vcpu_user_state(Vcpu_state *vcpu)
{
  _tpidruro = vcpu->_regs.tpidruro;
  load_tpidruro();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!arm_v6plus]:

PROTECTED inline void Context::arch_setup_utcb_ptr()
{}

PRIVATE inline
void
Context::store_tpidrurw() const
{}

PRIVATE inline
void
Context::load_tpidrurw() const
{}

PROTECTED inline
void
Context::load_tpidruro() const
{}

PUBLIC inline
Mword
Context::tpidrurw() const
{
  return 0;
}

PUBLIC inline
Mword
Context::tpidruro() const
{
  return 0;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

#include "kmem.h"

PUBLIC
void
Context::init_mpu_state()
{
  auto const &kd = *Kmem::kdir;
  _mpu_prbar2 = kd[Kpdir::Kernel_heap].prbar;
  _mpu_prlar2 = kd[Kpdir::Kernel_heap].prlar;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !mpu]:

PUBLIC inline
void
Context::init_mpu_state()
{}
