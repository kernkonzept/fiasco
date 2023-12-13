INTERFACE [arm && cpu_virt]:

#include "hyp_vm_state.h"

EXTENSION class Context
{
public:
  typedef Hyp_vm_state Vm_state;

protected:
  Context_hyp _hyp;
};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt && irq_direct_inject]:

EXTENSION class Context
{
protected:
  /**
   * List of injected IRQs.
   *
   * Holds all Irq objects that are injected to the vCPU and have an LR
   * assigned. The size of this list is bounded by the number of GIC LRs.
   */
  Vcpu_irq_list _injected_irqs;

  /**
   * List of pending IRQs waiting for injection.
   *
   * This list is potentially unbounded because the user space can queue any
   * number of Irqs.
   */
  Vcpu_irq_list _pending_irqs;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

PROTECTED static inline
Context::Vm_state *
Context::vm_state(Vcpu_state *vs)
{ return offset_cast<Vm_state *>(vs, Config::Ext_vcpu_state_offset); }

PROTECTED inline
void
Context::sanitize_vmm_state(Return_frame *r) const
{
  r->psr_set_mode((_hyp.hcr & Cpu::Hcr_tge) ? Proc::Status_mode_user_el0
                                            : Proc::Status_mode_user_el1);
  r->psr |= 0x1c0; // mask PSTATE.{I,A,F}
}

IMPLEMENT_OVERRIDE
void
Context::arch_vcpu_ext_shutdown()
{
  if (!(state() & Thread_ext_vcpu_enabled))
    return;

  state_del_dirty(Thread_ext_vcpu_enabled);
  regs()->psr = Proc::Status_mode_user_el0;
  _hyp.hcr = Cpu::Hcr_non_vm_bits_el0;
  _hyp.load(/*from_privileged*/ true, /*to_privileged*/ false);
  arm_hyp_load_non_vm_state();
  Gic_h_global::gic->switch_to_non_vcpu(Gic_h::From_vgic_mode::Enabled);
}

IMPLEMENT_OVERRIDE inline NEEDS[Context::vm_state,
                                Context::arm_ext_vcpu_switch_to_host,
                                Context::arm_ext_vcpu_load_host_regs,
                                Context::arm_ext_vcpu_switch_to_host_no_load]
void
Context::arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load)
{
  if (!(state() & Thread_ext_vcpu_enabled))
    {
      _tpidruro = vcpu->host.tpidruro;
      if (do_load)
        load_tpidruro();
      return;
    }

  Vm_state *v = vm_state(vcpu);

  bool const all_priv_vm = !(_hyp.hcr & Cpu::Hcr_tge);
  if (all_priv_vm)
    {
      // save guest state, load full host state
      if (do_load)
        {
          arm_ext_vcpu_switch_to_host(vcpu, v);
          Gic_h_global::gic->save_and_disable(&v->gic);
        }
      else
        arm_ext_vcpu_switch_to_host_no_load(vcpu, v);
    }

  _tpidruro = vcpu->host.tpidruro;
  _hyp.hcr = access_once(&v->host_regs.hcr) | Cpu::Hcr_must_set_bits;
  if (do_load)
    arm_ext_vcpu_load_host_regs(vcpu, v, _hyp.hcr);
}

IMPLEMENT_OVERRIDE inline NEEDS[Context::vm_state,
                                Context::arm_ext_vcpu_switch_to_guest,
                                Context::arm_ext_vcpu_load_guest_regs,
                                Context::vcpu_vgic_switch_to_user]
void
Context::arch_load_vcpu_user_state(Vcpu_state *vcpu)
{

  if (!(state() & Thread_ext_vcpu_enabled))
    {
      _tpidruro = vcpu->_regs.tpidruro;
      load_tpidruro();
      return;
    }

  Vm_state *v = vm_state(vcpu);
  _hyp.hcr = access_once(&v->guest_regs.hcr) | Cpu::Hcr_must_set_bits;
  bool const all_priv_vm = !(_hyp.hcr & Cpu::Hcr_tge);

  if (all_priv_vm)
    {
      arm_ext_vcpu_switch_to_guest(vcpu, v);
      Gic_h_global::gic->switch_to_vcpu(&v->gic,
                                        Gic_h::To_user_mode::Enabled,
                                        Gic_h::From_vgic_mode::Enabled);
      vcpu_vgic_switch_to_user(vcpu);
    }

  arm_ext_vcpu_load_guest_regs(vcpu, v, _hyp.hcr);
  _tpidruro          = vcpu->_regs.tpidruro;
}

PUBLIC inline NEEDS[Context::arm_hyp_load_non_vm_state,
                    Context::vm_state,
                    Context::store_tpidruro,
                    Context::load_cnthctl,
                    Context::save_ext_vcpu_state,
                    Context::load_ext_vcpu_state]
void
Context::switch_vm_state(Context *t)
{
  Mword _state = state();
  Mword _to_state = t->state();

  bool const from_ext_vcpu_enabled = _state & Thread_ext_vcpu_enabled;
  bool const from_all_priv = !(_hyp.hcr & Cpu::Hcr_tge);
  bool const to_ext_vcpu_enabled = _to_state & Thread_ext_vcpu_enabled;
  bool const to_all_priv = !(t->_hyp.hcr & Cpu::Hcr_tge);

  _hyp.save(from_all_priv || from_ext_vcpu_enabled);
  store_tpidruro();

  t->_hyp.load(from_all_priv || from_ext_vcpu_enabled,
               to_all_priv || to_ext_vcpu_enabled);

  if (!from_ext_vcpu_enabled && !to_ext_vcpu_enabled)
    {
      if (space()->has_gicc_page_mapped())
        // gicv3 will only act on Gic_h::From_vgic_mode::Enabled
        Gic_h_global::gic->switch_to_non_vcpu(Gic_h::From_vgic_mode::Disabled);
      return;
    }

  // either current or next has extended vCPU enabled

  Gic_h::From_vgic_mode from_mode = Gic_h::From_vgic_mode::Disabled;

  if (from_ext_vcpu_enabled)
    {
      Vm_state *v = vm_state(vcpu_state().access());
      save_ext_vcpu_state(_state, v);

      if (_state & Thread_vcpu_user)
        from_mode = Gic_h_global::gic->switch_from_vcpu(&v->gic);
    }

  if (to_ext_vcpu_enabled)
    {
      Vm_state const *v = vm_state(t->vcpu_state().access());
      t->load_ext_vcpu_state(_to_state, v);

      Gic_h_global::gic->switch_to_vcpu(&v->gic,
                                        (_to_state & Thread_vcpu_user)
                                          ? Gic_h::To_user_mode::Enabled
                                          : Gic_h::To_user_mode::Disabled,
                                        from_mode);
    }
  else
    {
      arm_hyp_load_non_vm_state();
      // Need to do this always:
      // - GICv2: handle switching to a thread which can access the vGIC page
      //          but is not in extended vCPU mode
      // - GICv3: needs to be always called
      Gic_h_global::gic->switch_to_non_vcpu(from_mode);
    }

}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && irq_direct_inject]:

IMPLEMENT
void
Context::arch_inject_vcpu_irq(Mword irq_id, Vcpu_irq_list_item *irq)
{
  assert(home_cpu() == current_cpu() || !Cpu::online(home_cpu()));
  assert(cpu_lock.test());

  // This can only happen if an Irq is triggered again before the guest has
  // issued the EOI for it. There are two theoretical cases:
  //
  // 1) An LR was allocated, the `irq` is on the _injected_irqs list and
  //    vcpu_soi() was called. The guest might, or might not have, acknowledged
  //    the interrupt (pending vs. active LR state). If the LR is not active
  //    yet, we could coalesce the interrupt but that would open up the
  //    possibility of an interrupt storm:
  //      * A device is constantly generating interrupts
  //      * The guest is in vCPU user mode but preempted
  //    Then the LR will not get active and the hardware interrupt would not be
  //    masked either. So we are conservative and let the additional interrupt
  //    be pending. The guest might experience an additional, unexpected
  //    interrupt after the current one, though.
  // 2) No LR was allocated yet (on _pending_irqs list). This should not be
  //    possible. The Irq_sender should only ever call us on the Idle->Queued
  //    transition and without a callback to vcpu_soi(), this transition cannot
  //    happen twice.
  if (EXPECT_FALSE(Vcpu_irq_list::in_list(irq)))
    {
      assert(irq->lr && !irq->queued);
      irq->queued = true;
      return;
    }

  assert(!irq->lr);

  if ((state() & (Thread_vcpu_user | Thread_ext_vcpu_enabled)) ==
                 (Thread_vcpu_user | Thread_ext_vcpu_enabled))
    {
      // We've interrupted the guest and can directly inject the IRQ
      Vcpu_state *vcpu = vcpu_state().access();
      Vm_state *v = vm_state(vcpu);
      irq->lr = Gic_h_global::gic->inject(&v->gic, Gic_h::Vcpu_irq_cfg(irq_id),
                                          current() == this);
      if (irq->lr)
        {
          _injected_irqs.push_back(irq);
          irq->vcpu_soi();
        }
      else
        _pending_irqs.push_back(irq);
    }
  else
    {
      _pending_irqs.push_back(irq);

      // Attention: this *must* be the last statement because it will usually
      // take the Irq_shortcut straight back to user space!
      if (Irq_base *doorbell = access_once(&_doorbell_irq))
        doorbell->hit(0);
    }
}

IMPLEMENT
Context::Revoke_vcpu_state
Context::arch_revoke_vcpu_irq(Vcpu_irq_list_item *irq, bool abandon)
{
  assert(home_cpu() == current_cpu() || !Cpu::online(home_cpu()));
  assert(cpu_lock.test());

  if (!Vcpu_irq_list::in_list(irq))
    return Revoke_vcpu_state::Ok_was_clear;

  if (irq->lr)
    {
      Vcpu_state *vcpu = vcpu_state().access();
      Vm_state *v = vm_state(vcpu);
      bool load = current() == this;
      bool active = Gic_h_global::gic->revoke(&v->gic, irq->lr - 1U, load,
                                              abandon);
      if (EXPECT_FALSE(active && !abandon))
        return Revoke_vcpu_state::Fail_is_active;

      bool queued = irq->queued;
      irq->lr = 0;
      irq->queued = false;
      _injected_irqs.remove(irq);

      if (!active)
        return Revoke_vcpu_state::Ok_was_pending;

      return queued ? Revoke_vcpu_state::Ok_was_active_and_queued
                    : Revoke_vcpu_state::Ok_was_active;
    }
  else
    {
      _pending_irqs.remove(irq);
      return Revoke_vcpu_state::Ok_was_queued;
    }
}

PROTECTED
void
Context::vcpu_handle_pending_injects(Vm_state *v)
{
  assert(cpu_lock.test());

  auto it = _pending_irqs.begin();
  while (it != _pending_irqs.end())
    {
      Vcpu_irq_list_item *irq = *it;
      assert(!irq->lr);
      irq->lr = Gic_h_global::gic->inject(&v->gic,
                                          Gic_h::Vcpu_irq_cfg(irq->vcpu_irq_id()),
                                          true);
      if (!irq->lr)
        break;

      it = _pending_irqs.erase(it);
      _injected_irqs.push_back(irq);
      irq->vcpu_soi();
    }
}

/**
 * Handle vGIC maintenance of kernel injected interrupts.
 *
 * All EOIs of interrupts that belong to the kernel need to be handled
 * before returning to vCPU kernel mode.
 *
 * \retval true  if an upcall to vCPU kernel mode should be made.
 * \retval false if the thread should stay in vCPU user mode.
 */
PROTECTED inline
bool
Context::vcpu_vgic_maintenance()
{
  Vcpu_state *vcpu = vcpu_state().access();
  Vm_state *v = vm_state(vcpu);

  Unsigned32 eois;
  bool upcall = !Gic_h_global::gic->handle_maintenance(&v->gic, &eois);
  if (eois)
    {
      auto it = _injected_irqs.begin();
      while (it != _injected_irqs.end())
        {
          Vcpu_irq_list_item *irq = *it;
          if (eois & (1UL << (irq->lr - 1U)))
            {
              it = _injected_irqs.erase(it);
              irq->vcpu_eoi();
              irq->lr = 0;
              if (EXPECT_FALSE(irq->queued))
                {
                  irq->queued = false;
                  _pending_irqs.push_back(irq);
                }
            }
          else
            ++it;
        }
    }

  if (!upcall)
    vcpu_handle_pending_injects(v);

  return upcall;
}

PRIVATE inline
void
Context::vcpu_vgic_switch_to_user(Vcpu_state *vcpu)
{
  vcpu_handle_pending_injects(vm_state(vcpu));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && !irq_direct_inject]:

PROTECTED inline
bool
Context::vcpu_vgic_maintenance()
{ return true; }

PRIVATE inline
void
Context::vcpu_vgic_switch_to_user(Vcpu_state *)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC inline void Context::switch_vm_state(Context *) {}
