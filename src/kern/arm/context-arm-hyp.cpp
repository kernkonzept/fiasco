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

EXTENSION class Context
{
protected:
  enum
  {
    Host_cntkctl = (1U << 8) | (1U << 1),
    // see: generic_timer.cpp: setup_timer_access (Hyp)
    Host_cnthctl = 1U,  // enable PL1+PL0 access to CNTPCT_EL0
    Guest_cnthctl = 0U, // disable PL1+PL0 access to CNTPCT_EL0
  };
};

PROTECTED static inline
Context::Vm_state *
Context::vm_state(Vcpu_state *vs)
{ return reinterpret_cast<Vm_state *>(reinterpret_cast<char *>(vs) + 0x400); }

IMPLEMENT_OVERRIDE
void
Context::arch_vcpu_ext_shutdown()
{
  if (!(state() & Thread_ext_vcpu_enabled))
    return;

  state_del_dirty(Thread_ext_vcpu_enabled);
  _hyp.hcr = Cpu::Hcr_non_vm_bits;
  arm_hyp_load_non_vm_state(true);
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
      // vCPU user state has TGE set, so we need to reload HCR here
      _hyp.hcr = Cpu::Hcr_non_vm_bits;
      if (do_load)
        {
          Cpu::hcr(_hyp.hcr);
          load_tpidruro();
        }
      return;
    }

  Vm_state *v = vm_state(vcpu);

  v->guest_regs.hcr = _hyp.hcr;
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
  _hyp.hcr = Cpu::Hcr_host_bits;
  if (do_load)
    arm_ext_vcpu_load_host_regs(vcpu, v);
}

IMPLEMENT_OVERRIDE inline NEEDS[Context::vm_state,
                                Context::arm_ext_vcpu_switch_to_guest,
                                Context::arm_ext_vcpu_switch_to_guest_no_load,
                                Context::arm_ext_vcpu_load_guest_regs]
void
Context::arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load)
{

  if (!(state() & Thread_ext_vcpu_enabled))
    {
      _hyp.hcr = Cpu::Hcr_non_vm_bits | Cpu::Hcr_tge;
      _tpidruro = vcpu->_regs.tpidruro;
      if (do_load)
        {
          Cpu::hcr(_hyp.hcr);
          load_tpidruro();
        }
      return;
    }

  Vm_state *v = vm_state(vcpu);
  _hyp.hcr = access_once(&v->guest_regs.hcr) | Cpu::Hcr_must_set_bits;
  bool const all_priv_vm = !(_hyp.hcr & Cpu::Hcr_tge);

  if (all_priv_vm)
    {
      if (do_load)
        {
          arm_ext_vcpu_switch_to_guest(vcpu, v);
          Gic_h_global::gic->load_full(&v->gic, true);
        }
      else
        arm_ext_vcpu_switch_to_guest_no_load(vcpu, v);
    }

  if (do_load)
    {
      arm_ext_vcpu_load_guest_regs(vcpu, v, _hyp.hcr);
      _tpidruro          = vcpu->_regs.tpidruro;
    }
  else
    {
      vcpu->host.tpidruro = _tpidruro;
      _tpidruro           = vcpu->_regs.tpidruro;
    }
}

PUBLIC inline NEEDS[Context::arm_hyp_load_non_vm_state,
                    Context::vm_state,
                    Context::store_tpidruro,
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
    return;

  // either current or next has extended vCPU enabled

  bool vgic = false;

  if (_state & Thread_ext_vcpu_enabled)
    {
      Vm_state *v = vm_state(vcpu_state().access());
      save_ext_vcpu_state(_state, v);

      if ((_state & Thread_vcpu_user))
        vgic = Gic_h_global::gic->save_full(&v->gic);
    }

  if (_to_state & Thread_ext_vcpu_enabled)
    {
      Vm_state const *v = vm_state(t->vcpu_state().access());
      t->load_ext_vcpu_state(_to_state, v);

      if (_to_state & Thread_vcpu_user)
        Gic_h_global::gic->load_full(&v->gic, vgic);
      else if (vgic)
        Gic_h_global::gic->disable();
    }
  else
    arm_hyp_load_non_vm_state(vgic);
}

IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC inline void Context::switch_vm_state(Context *) {}
