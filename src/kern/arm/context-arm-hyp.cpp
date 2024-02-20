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
IMPLEMENTATION [arm && cpu_virt]:

PROTECTED static inline
Context::Vm_state *
Context::vm_state(Vcpu_state *vs)
{ return offset_cast<Vm_state *>(vs, 0x400); }

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
  Cpu::hcr(_hyp.hcr);
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
                                Context::arm_ext_vcpu_load_guest_regs]
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
  _hyp.save();
  store_tpidruro();
  t->_hyp.load();

  Mword _state = state();
  Mword _to_state = t->state();
  if (!((_state | _to_state) & Thread_ext_vcpu_enabled))
    {
      Gic_h_global::gic->switch_to_non_vcpu(Gic_h::From_vgic_mode::Disabled);
      return;
    }

  // either current or next has extended vCPU enabled

  Gic_h::From_vgic_mode from_mode = Gic_h::From_vgic_mode::Disabled;

  if (_state & Thread_ext_vcpu_enabled)
    {
      Vm_state *v = vm_state(vcpu_state().access());
      save_ext_vcpu_state(_state, v);

      if (_state & Thread_vcpu_user)
        from_mode = Gic_h_global::gic->switch_from_vcpu(&v->gic);
    }

  if (_to_state & Thread_ext_vcpu_enabled)
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
      Gic_h_global::gic->switch_to_non_vcpu(from_mode);
    }

}

IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC inline void Context::switch_vm_state(Context *) {}
