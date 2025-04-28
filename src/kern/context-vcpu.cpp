INTERFACE:

#include "vcpu.h"

EXTENSION class Context
{
public:
  enum class Revoke_vcpu_state
  {
    /// The vIRQ was neither pending nor active on the vCPU.
    Ok_was_clear,
    /// The vIRQ was queued on the vCPU but not taken over yet. In particular,
    /// vcpu_soi() was not called yet.
    Ok_was_queued,
    /// The vIRQ was taken over (vcpu_soi() had been called) and was visible to
    /// the guest. It was not active on the vCPU, though, that is the guest had
    /// not yet acknowledged the interrupt.
    Ok_was_pending,
    /// The vIRQ was active on the vCPU and has been abandoned.
    Ok_was_active,
    /// The vIRQ was active on the vCPU, has been abandoned and was already
    /// queued again.
    Ok_was_active_and_queued,
    /// The vIRQ is currently active on the vCPU and has *not* been revoked!
    Fail_is_active,
  };

  void vcpu_pv_switch_to_kernel(Vcpu_state *, bool);
  void vcpu_pv_switch_to_user(Vcpu_state *, bool);

  /**
   * Inject vIRQ into the vCPU.
   *
   * If possible, it will be inserted directly. If the thread is not in vCPU
   * user mode, the doorbell irq will be triggered to tell the VMM that an irq
   * was queued.
   *
   * \pre Called only on home CPU of the context.
   *
   * \attention If the context is not currently in vcpu user mode, the function
   *            may take the Irq_shortcut. This implies that
   *            arch_inject_vcpu_irq() might return only after rescheduling!
   *
   * \param irq_id  The arch specific guest interrupt specification
   * \param irq     The Irq object that is possibly pending.
   */
  void arch_inject_vcpu_irq(Mword irq_id, Vcpu_irq_list_item *irq);

  /**
   * Revoke a possibly injected vIRQ from the vCPU.
   *
   * If `abandon` is true, the interrupt is removed unconditionally, regardless
   * of its current state. In case it was active (i.e. guest has acknowledged
   * but not yet EOIed), it will stay active and the VMM has to cope with the
   * eventual EOI of the guest. Otherwise, if the vIRQ is currently active, the
   * function will fail.
   *
   * \pre Called only on home CPU of the context.
   *
   * \param irq      The Irq object that is possibly pending.
   * \param abandon  Abandon Irq if it's active on the vCPU.
   *
   * \return  The state after revoking the vIRQ.
   */
  Revoke_vcpu_state arch_revoke_vcpu_irq(Vcpu_irq_list_item *irq, bool abandon);

protected:
  Ku_mem_ptr<Vcpu_state> _vcpu_state;
};

// ---------------------------------------------------------------------
INTERFACE [irq_direct_inject]:

#include "observer_irq.h"

EXTENSION class Context
{
protected:
  Observer_irq _doorbell_irq;
};

// ---------------------------------------------------------------------
IMPLEMENTATION [!fpu]:

PROTECTED inline
void
Context::vcpu_enable_fpu_if_disabled(Mword)
{}

// ---------------------------------------------------------------------
IMPLEMENTATION [fpu && lazy_fpu]:

PROTECTED inline
void
Context::vcpu_enable_fpu_if_disabled(Mword thread_state)
{
  if ((thread_state & (Thread_fpu_owner | Thread_vcpu_fpu_disabled))
      == (Thread_fpu_owner | Thread_vcpu_fpu_disabled))
    Fpu::fpu.current().enable();
}

// ---------------------------------------------------------------------
IMPLEMENTATION [fpu && !lazy_fpu]:

PROTECTED inline
void
Context::vcpu_enable_fpu_if_disabled(Mword thread_state)
{
  if (thread_state & Thread_vcpu_fpu_disabled)
    Fpu::fpu.current().enable();
}

// ---------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT_DEFAULT inline
void Context::vcpu_pv_switch_to_kernel(Vcpu_state *, bool) {}

IMPLEMENT_DEFAULT inline
void Context::vcpu_pv_switch_to_user(Vcpu_state *, bool) {}

PUBLIC inline
Context::Ku_mem_ptr<Vcpu_state> const &
Context::vcpu_state() const
{ return _vcpu_state; }


PUBLIC inline
Mword
Context::vcpu_disable_irqs()
{
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    {
      Vcpu_state *vcpu = vcpu_state().access();
      Mword s = vcpu->state;
      vcpu->state = s & ~Vcpu_state::F_irqs;
      return s & Vcpu_state::F_irqs;
    }
  return 0;
}

PUBLIC inline
void
Context::vcpu_restore_irqs(Mword irqs)
{
  if (EXPECT_FALSE((irqs & Vcpu_state::F_irqs)
                   && (state() & Thread_vcpu_enabled)))
    vcpu_state().access()->state |= Vcpu_state::F_irqs;
}

PUBLIC inline
void
Context::vcpu_save_state_and_upcall()
{
  extern char upcall[] asm ("leave_by_vcpu_upcall");
  _exc_cont.activate(regs(), upcall);
}

PUBLIC inline
void
Context::vcpu_save_state_and_upcall_async_ipc()
{
  extern char upcall_async_ipc[] asm ("leave_by_vcpu_upcall_async_ipc");
  _exc_cont.activate(regs(), upcall_async_ipc);
}

PUBLIC inline NEEDS["fpu.h", "space.h",
                    Context::vcpu_enable_fpu_if_disabled,
                    Context::arch_load_vcpu_kern_state,
                    Context::vcpu_pv_switch_to_kernel]
bool
Context::vcpu_enter_kernel_mode(Vcpu_state *vcpu)
{
  unsigned s = state();
  if (EXPECT_FALSE(s & Thread_vcpu_enabled))
    {
      vcpu->_saved_state = vcpu->state;
      Mword flags = Vcpu_state::F_traps
	            | Vcpu_state::F_user_mode;
      vcpu->state &= ~flags;

      if (vcpu->_saved_state & Vcpu_state::F_user_mode)
	vcpu->_sp = vcpu->_entry_sp;
      else
	vcpu->_sp = regs()->sp();

      if (s & Thread_vcpu_user)
	{
          state_del_dirty(Thread_vcpu_user | Thread_vcpu_fpu_disabled);

          bool load_cpu_state = current() == this;
          arch_load_vcpu_kern_state(vcpu, load_cpu_state);
          vcpu_pv_switch_to_kernel(vcpu, load_cpu_state);

          if (load_cpu_state)
            vcpu_enable_fpu_if_disabled(s);

          if (_space.user_mode())
            {
              _space.user_mode(false);
              if (load_cpu_state)
                {
                  // Space::switchin_context() may optimize the switch of a
                  // thread in vCPU user mode to vCPU kernel mode.
                  space()->switchin_context(vcpu_user_space(),
                                            Mem_space::Vcpu_user_to_kern);
                  return true;
                }
            }
        }
    }
  return false;
}



PUBLIC inline
bool
Context::vcpu_irqs_enabled(Vcpu_state *vcpu) const
{
  return EXPECT_FALSE(state() & Thread_vcpu_enabled)
    && vcpu->state & Vcpu_state::F_irqs;
}

PUBLIC inline
bool
Context::vcpu_pagefaults_enabled(Vcpu_state *vcpu) const
{
  return EXPECT_FALSE(state() & Thread_vcpu_enabled)
    && vcpu->state & Vcpu_state::F_page_faults;
}

PUBLIC inline
bool
Context::vcpu_exceptions_enabled(Vcpu_state *vcpu) const
{
  return EXPECT_FALSE(state() & Thread_vcpu_enabled)
    && vcpu->state & Vcpu_state::F_exceptions;
}

PUBLIC inline
void
Context::vcpu_set_irq_pending()
{
  if (EXPECT_FALSE(state() & Thread_vcpu_enabled))
    vcpu_state().access()->sticky_flags |= Vcpu_state::Sf_irq_pending;
}

PUBLIC inline
Space *
Context::vcpu_user_space() const
{ return _space.vcpu_user(); }


// --------------------------------------------------------------------------
IMPLEMENTATION [!irq_direct_inject]:

IMPLEMENT inline
void Context::arch_inject_vcpu_irq(Mword, Vcpu_irq_list_item *)
{}

IMPLEMENT inline
Context::Revoke_vcpu_state
Context::arch_revoke_vcpu_irq(Vcpu_irq_list_item *, bool)
{ return Context::Revoke_vcpu_state::Ok_was_clear; }


// --------------------------------------------------------------------------
INTERFACE [debug]:

EXTENSION class Context
{
  static unsigned vcpu_log_fmt(Tb_entry *, int, char *)
  asm ("__context_vcpu_log_fmt");
};


// --------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "kobject_dbg.h"
#include "string_buffer.h"

IMPLEMENT
void
Context::Vcpu_log::print(String_buffer *buf) const
{
  switch (type)
    {
    case 0:
    case 4:
      buf->printf("%sret pc=%lx sp=%lx state=%lx task=D:%lx",
                  type == 4 ? "f" : "", ip, sp, state, space);
      break;
    case 1:
      buf->printf("ipc from D:%lx task=D:%lx sp=%lx",
                  Kobject_dbg::pointer_to_id(reinterpret_cast<Kobject*>(ip)),
                  state, sp);
      break;
    case 2:
      buf->printf("exc #%x err=%lx pc=%lx sp=%lx state=%lx task=D:%lx",
                  trap, err, ip, sp, state, space);
      break;
    case 3:
      buf->printf("pf  pc=%lx pfa=%lx err=%lx state=%lx task=D:%lx",
                  ip, sp, err, state, space);
      break;
    default:
      buf->printf("vcpu: unknown");
      break;
    }
}

