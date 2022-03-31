INTERFACE [arm && cpu_virt]:

#include "hyp_vm_state.h"

EXTENSION class Context
{
public:
  typedef Hyp_vm_state Vm_state;

protected:
  Context_hyp _hyp;

  class Vtimer_vcpu_irq final : public Vcpu_irq_list_item
  {
    Hyp_vm_state::Vtmr _vtmr;

  public:
    bool vcpu_eoi() override
    {
      _vtmr.pending() = 0;
      return false;
    }

    Mword vcpu_irq_id() const override
    { return _vtmr.raw & Hyp_vm_state::Vtmr::Vcpu_irq_cfg_mask; }

    void load(Hyp_vm_state::Vtmr vtmr)
    { _vtmr = vtmr; }

    void save(Hyp_vm_state::Vtmr *vtmr) const
    { *vtmr = _vtmr; }

    bool set_pending()
    {
      _vtmr.pending() = 1;
      return _vtmr.direct() && _vtmr.enabled();
    }

    bool upcall() const { return !_vtmr.direct(); }

    bool queue() const
    { return _vtmr.direct() && _vtmr.enabled() && _vtmr.pending(); }

    // Re-enable vtimer PPI? This implies a single GIC register write which is
    // cheap. Hence we only want to prevent re-enabling the PPI if it should
    // definitely not be taken.
    bool reenable_ppi() const
    { return !(_vtmr.direct() && _vtmr.enabled() && _vtmr.pending()); }
  };

  Vcpu_irq_list _injected_irqs;
  Mword _pending_injections;
  Vtimer_vcpu_irq _vtimer_irq;
  Unsigned8 _vtimer_irq_priority;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "irq.h"
#include "irq_mgr.h"

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
{
  return reinterpret_cast<Vm_state *>(reinterpret_cast<char *>(vs)
                                      + Config::Ext_vcpu_state_offset);
}

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
      _irq_priority = sched_context()->prio();
      if (do_load)
        {
          arm_ext_vcpu_switch_to_host(vcpu, v);
          Gic_h_global::gic->save_and_disable(&v->gic);
          Irq_mgr::mgr->set_priority_mask(_irq_priority);
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

          _vtimer_irq_priority = access_once(&v->vtmr).host_prio();
          if (_vtimer_irq_priority > _irq_priority)
            _vtimer_irq_priority = _irq_priority;

          auto vtimer = Irq_mgr::mgr->chip(27);
          vtimer.chip->set_priority_percpu(current_cpu(), vtimer.pin,
                                           _vtimer_irq_priority);

          Unsigned8 irq_prio = Gic_h_global::gic->load_full(&v->gic, true);
          if (irq_prio < _irq_priority)
            {
              _irq_priority = irq_prio;
              Irq_mgr::mgr->set_priority_mask(irq_prio);
            }
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
      auto vtimer = Irq_mgr::mgr->chip(27);
      vtimer.chip->set_priority_percpu(current_cpu(), vtimer.pin,
                                       t->_vtimer_irq_priority);

      if (_to_state & Thread_vcpu_user)
        Gic_h_global::gic->load_full(&v->gic, vgic);
      else if (vgic)
        Gic_h_global::gic->disable();
    }
  else
    arm_hyp_load_non_vm_state(vgic);
}

PROTECTED inline
void
Context::init_virt_state()
{
  _doorbell_irq = 0;
  _pending_injections = 0;
}

/**
 * Adjust the threads irq priority to a newly injected interrupt.
 *
 * \pre The thread is currently running.
 */
PRIVATE
void
Context::adjust_irq_priority(Unsigned8 injected_prio)
{
  Unsigned8 max_priority = sched_context()->prio();
  if (EXPECT_FALSE(injected_prio >= max_priority))
    injected_prio = max_priority;

  if (injected_prio > _irq_priority)
    {
      _irq_priority = injected_prio;
      Irq_mgr::mgr->set_priority_mask(injected_prio);
    }
}

/**
 * Re-calculate the current irq priority from pending/active virqs.
 *
 * \pre The thread is currently running.
 */
PROTECTED
void
Context::recalculate_irq_priority(Vm_state *v)
{
  Unsigned8 irq_prio = Gic_h_global::gic->calc_irq_priority(&v->gic);
  Unsigned8 max_priority = sched_context()->prio();
  if (irq_prio >= max_priority)
    irq_prio = max_priority;

  if (irq_prio != _irq_priority)
    {
      _irq_priority = irq_prio;
      Irq_mgr::mgr->set_priority_mask(irq_prio);
    }
}

/**
 * Inject vIRQ into the vCPU.
 *
 * If possible it will be inserted directly. If the thread is not in vCPU user
 * mode the doorbell irq will be triggered to tell the vmm that an irq was
 * queued.
 *
 * \attention If the context is not currently in vcpu user mode the function
 *            may take the Irq_shortcut. This implies that
 *            arch_inject_vcpu_irq() might not always return!
 */
IMPLEMENT_OVERRIDE
void
Context::arch_inject_vcpu_irq(Mword irq_id, Vcpu_irq_list_item *irq)
{
  assert(cpu_lock.test());

  auto g = lock_guard(_injected_irqs.lock());
  if(_injected_irqs.in_list(irq))
    return;

  assert(!irq->lr);
  _injected_irqs.push_front(irq);

  if ((state() & (Thread_vcpu_user | Thread_ext_vcpu_enabled)) ==
                 (Thread_vcpu_user | Thread_ext_vcpu_enabled))
    {
      // We've interrupted the guest and can directly inject the IRQ
      Vcpu_state *vcpu = vcpu_state().access();
      Vm_state *v = vm_state(vcpu);
      bool load = current() == this;
      Unsigned8 irq_prio;
      irq->lr = Gic_h_global::gic->inject(&v->gic, Gic_h::Vcpu_irq_cfg(irq_id),
                                          load, &irq_prio);
      if (!irq->lr)
        // TODO: preempt an LR if this irq has higher priority
        // TODO: we should still raise the threads irq priority
        _pending_injections++;
      else if (load)
        adjust_irq_priority(irq_prio);
    }
  else
    {
      _pending_injections++;

      // Attention: this *must* be the last statement because it will usually
      // take the Irq_shortcut straight back to user space!
      g.reset();
      if (Irq_base *doorbell = access_once(&_doorbell_irq))
        doorbell->hit(0);
    }
}

/**
 * Revoke a possibly injected vIRQ from the vCPU.
 *
 * If \p reap is true the interrupt is removed unconditionally, regardless of
 * its current state. Otherwise, if the vIRQ is currently active, it will be
 * left active on the vCPU and eoi'ed once the vCPU is finshed with it (or the
 * thread dies).
 *
 * The pending/active irq should not be revoked from a foreign thread. We
 * cannot update the vCPU state in this case and the vIRQ will stay injected on
 * the the vCPU!
 *
 * \return True if irq was pending, false otherwise.
 */
IMPLEMENT_OVERRIDE
bool
Context::arch_revoke_vcpu_irq(Vcpu_irq_list_item *irq, bool reap)
{
  auto g = lock_guard(_injected_irqs.lock());
  if (!_injected_irqs.in_list(irq))
    return false;

  _injected_irqs.remove(irq);
  if (irq->lr)
    {
      if (current() != this)
        {
          irq->lr = 0;
          return true;
        }

      Vcpu_state *vcpu = vcpu_state().access();
      Vm_state *v = vm_state(vcpu);
      bool active = Gic_h_global::gic->revoke(&v->gic, irq->lr-1U, reap);
      irq->lr = 0;

      // FIXME: we somehow have to wait for the vCPU to eoi the interrupt
      // without having it queued here. Currently it will be unmasked too early
      // and the new vCPU will possibly get the IRQ in parallel!
      //if (active)
      //  queue_somewhere_else_and_wait_for_guest_eoi();
      //return !active;

      // FIXME: instead we re-insert it on the new vCPU :(
      (void)active;

      recalculate_irq_priority(v);
      return true;
    }
  else
    _pending_injections--;

  return false;
}

PROTECTED
void
Context::vcpu_handle_pending_injects(Vm_state *v, bool load)
{
  assert(cpu_lock.test());
  auto g = lock_guard(_injected_irqs.lock());

  if (EXPECT_TRUE(!_pending_injections))
    return;

  for (auto && irq : _injected_irqs)
    {
      if (irq->lr)
        continue;

      Unsigned8 irq_prio;
      irq->lr = Gic_h_global::gic->inject(&v->gic,
                                          Gic_h::Vcpu_irq_cfg(irq->vcpu_irq_id()),
                                          load, &irq_prio);
      if (irq->lr)
        {
          if (load)
            adjust_irq_priority(irq_prio);

          --_pending_injections;
          if (!_pending_injections)
            break;
        }
      else
        break;
    }
}

PROTECTED virtual
void
Context::vcpu_prepare_vtimer()
{}

IMPLEMENT_OVERRIDE inline
void Context::vcpu_pv_switch_to_user(Vcpu_state *vcpu, bool current)
{
  if (!(state() & Thread_ext_vcpu_enabled))
    return;

  check(current);
  assert(state() & Thread_vcpu_user);

  Vm_state *v = vm_state(vcpu);
  bool old_queued = _vtimer_irq.queue();
  _vtimer_irq.load(access_once(&v->vtmr));

  if (!old_queued && _vtimer_irq.queue())
    // This is guaranteed to *not* take the Irq_shortcut because the thread is
    // in vcpu user mode.
    arch_inject_vcpu_irq(_vtimer_irq.vcpu_irq_id(), &_vtimer_irq);
  else if (old_queued && !_vtimer_irq.queue())
    arch_revoke_vcpu_irq(&_vtimer_irq, false);

  vcpu_handle_pending_injects(v, current);
  vcpu_prepare_vtimer();
}

IMPLEMENT_OVERRIDE inline
void Context::vcpu_pv_switch_to_kernel(Vcpu_state *vcpu, bool)
{
  if (state() & Thread_ext_vcpu_enabled)
    _vtimer_irq.save(&vm_state(vcpu)->vtmr);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC inline void Context::switch_vm_state(Context *) {}

PROTECTED inline
void
Context::init_virt_state()
{}
