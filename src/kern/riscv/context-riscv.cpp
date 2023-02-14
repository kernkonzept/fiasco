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

  // switch to our page directory if necessary
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());
}

IMPLEMENT inline NEEDS["asm_riscv.h", "cpu.h"]
void
Context::switch_cpu(Context *t)
{
  update_consumed_time();

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
