INTERFACE [riscv && cpu_virt]:

EXTENSION class Context
{
protected:
  // must not collide with any bit in Thread_state
  enum
  {
    /**
     * Indicates whether a thread in extended vCPU mode has a valid guest space
     * (Vm kernel object). Used when switching to the VMM to decide whether to
     * switch in the guest space and enable hypervisor load/store instructions.
     */
    Thread_ext_vcpu_has_guest_space = 0x4000000,
  };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "asm_riscv.h"
#include "cpu.h"

#include <cassert>

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h", "entry_frame.h"]
Entry_frame *
Context::regs() const
{
  // If entry frame size is not a multiple of stack alignment, it is padded from
  // the bottom of the stack to adjust its start to the required stack alignment.
  return reinterpret_cast<Entry_frame *>(Cpu::stack_align(
    reinterpret_cast<Mword>(this) + Size - sizeof(Entry_frame)));
}

PUBLIC inline
void
Context::prepare_switch_to(void (*fptr)())
{
  assert(Cpu::is_stack_aligned(reinterpret_cast<Mword>(_kernel_sp)));
  // Make space for return address, maintain stack alignment.
  _kernel_sp -= Cpu::STACK_ALIGNMENT / sizeof(Mword);
  // Context::switch_cpu() loads return address from top of stack.
  *reinterpret_cast<void(**)()> (_kernel_sp) = fptr;
}

IMPLEMENT inline
void
Context::spill_user_state()
{}

IMPLEMENT inline
void
Context::fill_user_state()
{}

PROTECTED inline
void
Context::arch_setup_utcb_ptr()
{
  // Simulate the TLS model + TCB layout:
  // We store the UTCB pointer immediately before the TCB.
  // Until the userland setups its TCB, we use the UTCB's utcb_addr
  // as the UTCB pointer.
  regs()->tp =
    reinterpret_cast<Address>(&_utcb.usr()->utcb_addr) + (3 * sizeof(void*));
  _utcb.access()->utcb_addr = reinterpret_cast<Mword>(utcb().usr().get());
}

/** Thread context switchin.  Called on every re-activation of a thread
    (switch_exec()).  This method is public only because it is called from
    from assembly code in switch_cpu().
 */
IMPLEMENT
void
Context::switchin_context(Context *from)
{
  assert (this == current());
  assert (state() & Thread_ready_mask);

  from->handle_lock_holder_preemption();

  // switch in guest context if any
  switchin_guest_context();

  // switch to our page directory
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());
}

IMPLEMENT inline NEEDS[Context::switch_hyp_ext_state, Context::switch_gp_regs]
void
Context::switch_cpu(Context *t)
{
  switch_hyp_ext_state(t);
  switch_gp_regs(t);
}

PRIVATE inline NEEDS["asm_riscv.h", "cpu.h"]
void
Context::switch_gp_regs(Context *t)
{
  // Arguments for t->switchin_context(this)
  register Context *new_this asm("a0") = t;
  register Context *old_this asm("a1") = this;
  register Mword *new_sp asm("t0") = t->_kernel_sp;
  register Mword **old_sp asm("t1") = &_kernel_sp;

  __asm__ __volatile__
    (
      // Save state of old context
      "addi sp, sp, -%[stack_align] \n"
      "la t2, 1f                    \n"
      REG_S " t2, 0(sp)             \n"
#ifndef CONFIG_NO_FRAME_PTR
      // Save frame pointer.
      REG_S " s0, %[reg_size](sp)   \n"
#endif
      REG_S " sp, (%[old_sp])       \n"

      // Switch to new stack
      "mv sp, %[new_sp]             \n"

      // Switch to new context
      "jal switchin_context_label   \n"

      // Return to saved context entry point
      // (either from a previous context switch or Context::prepare_switch_to)
      REG_L " t2, 0(sp)             \n"
#ifndef CONFIG_NO_FRAME_PTR
      // Restore frame pointer.
      REG_L " s0, %[reg_size](sp)   \n"
#endif
      "addi sp, sp, %[stack_align]  \n"
      "jr t2                        \n"

      // Reentry for next switch to the old context
      "1:                           \n"
      : // Only read during context switch but may be changed by target context
        [old_sp]   "+r" (old_sp),
        [new_sp]   "+r" (new_sp),
        [new_this] "+r" (new_this),
        [old_this] "+r" (old_this)
      : [stack_align] "n" (Cpu::STACK_ALIGNMENT),
        [reg_size] "n" (SZREG)
      : // Inform the compiler that potentially all registers can change
        /* "a0", "a1", */ "a2", "a3", "a4", "a5", "a6", "a7",
        /* "t0", "t1", */ "t2", "t3", "t4", "t5", "t6",
#ifdef CONFIG_NO_FRAME_PTR
        // When compiling without frame pointer just clobber the register.
        "s0" /* = fp */,
#endif
        "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10",
        "s11",
        "ra", "gp", "tp", "memory"
    );
}

/**
 * Save the current values of the vCPU host mode preserved registers into the
 * given vCPU state.
 */
PUBLIC inline
void
Context::vcpu_save_host_regs(Vcpu_state *vcpu)
{
  vcpu->host.tp = regs()->tp;
  vcpu->host.gp = regs()->gp;
  vcpu->host.saved = true;
}

/**
 * Conditionally restore the vCPU host mode preserved registers saved in the
 * vCPU state of this context into the given Return_frame.
 */
PUBLIC inline
void
Context::vcpu_restore_host_regs(Return_frame *r)
{
  Vcpu_state *vcpu = vcpu_state().access();
  // Only restore the preserved vCPU host mode registers if they previously were
  // saved because the vCPU was switched to vCPU user mode.
  if (vcpu->host.saved)
    {
      r->tp = vcpu->host.tp;
      r->gp = vcpu->host.gp;
      vcpu->host.saved = false;
    }
}

/**
 * Copy the current values of the vCPU host mode preserved registers into the
 * given vCPU state.
 */
PUBLIC inline
void
Context::vcpu_copy_host_regs(Vcpu_state *vcpu)
{
  vcpu->_regs.s.tp = regs()->tp;
  vcpu->_regs.s.gp = regs()->gp;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !cpu_virt]:

PRIVATE inline
bool
Context::switchin_guest_context()
{ return false; }

PRIVATE inline
void
Context::switch_hyp_ext_state(Context *)
{}

//----------------------------------------------------------------------------
INTERFACE [riscv && cpu_virt]:

#include "hyp_ext_state.h"

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

PUBLIC static inline
Hyp_ext_state *
Context::vm_state(Vcpu_state *vs)
{
  static_assert(sizeof(Vcpu_state) <= 0x400, "vCPU state is too large.");
  return offset_cast<Hyp_ext_state *>(vs,  0x400);
}

PROTECTED inline
void
Context::switchin_guest_context()
{
  Mword _state = state();
  // Extended vCPU is enabled
  if (_state & Thread_ext_vcpu_enabled)
    {
      // Extended vCPU is in guest mode (VM)
      if (_state & Thread_vcpu_user)
        {
          // Switch to guest space
          assert(_state & Thread_ext_vcpu_has_guest_space);
          vcpu_user_space()->switchin_guest_space();
          return;
        }
      else if (_state & Thread_ext_vcpu_has_guest_space)
        {
          // Load g-stage page table for the VMM: hgatp and hstatus must be set
          // properly, even when switching to vCPU host mode, so that the user
          // mode VMM can use the hypervisor load/store instructions.
          vcpu_user_space()->switchin_guest_space();
          return;
        }
    }

  // Reset the current guest space, when switching to non-virtualization enabled
  // context or VMM without guest space. Even though the current context cannot
  // interact with the guest space, this is necessary for the case that the vCPU
  // belonging to the guest space is migrated to another CPU.
  Mem_space::reset_guest_space();
}

PRIVATE inline
void
Context::switch_hyp_ext_state(Context *to)
{
  Mword _state = state();
  Mword _to_state = to->state();

  if ((_state & Thread_ext_vcpu_enabled) && (_state & Thread_vcpu_user))
    vm_state(vcpu_state().access())->save();

  if ((_to_state & Thread_ext_vcpu_enabled))
    {
      if (_to_state & Thread_vcpu_user)
        vm_state(to->vcpu_state().access())->load();
      else
        vm_state(to->vcpu_state().access())->load_vmm();
    }
}

IMPLEMENT_OVERRIDE inline NEEDS[Context::vm_state]
void Context::vcpu_pv_switch_to_kernel(Vcpu_state *vcpu, bool current)
{
  if (!(state() & Thread_ext_vcpu_enabled))
    return;

  // Coming from guest?
  if (regs()->virt_mode())
    {
      assert(vcpu_user_space() != nullptr);
      // A vCPU running in vCPU user mode must always have a vCPU user space.
      assert(state() & Thread_ext_vcpu_has_guest_space);

      // Disable virtualization mode on VM exit, but enable hypervisor
      // load/store instructions for use by the VMM.
      regs()->virt_mode(false);
      regs()->allow_vmm_hyp_load_store(true);

      if (current)
        // Save VM state from architectural registers in vCPU state so it is
        // accessible to the VMM.
        vm_state(vcpu)->save();
    }
}
