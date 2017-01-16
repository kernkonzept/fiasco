IMPLEMENTATION [arm]:

PUBLIC inline
void
Context::prepare_switch_to(void (*fptr)())
{
  _kernel_sp -= 2;
  *reinterpret_cast<void (**)()> (_kernel_sp) = fptr;
}

PRIVATE inline void
Context::arm_switch_gp_regs(Context *t)
{
  register Mword _old_this asm("x1") = (Mword)this;
  register Mword _new_this asm("x0") = (Mword)t;
  register unsigned long dummy1 asm ("x9");
  register unsigned long dummy2 asm ("x10");

  asm volatile
    (// save context of old thread
     "   adr   x30, 1f            \n"
     "   str   x30, [sp, #-16]!   \n"
     "   str   x29, [sp, #8]      \n" // FP
     "   mov   x29, sp            \n"
     "   str   x29, [%[old_sp]]   \n"

     // switch to new stack
     "   mov   sp, %[new_sp]      \n"

     // deliver requests to new thread
     "   bl switchin_context_label \n" // call Context::switchin_context(Context *)

     // return to new context
     "   ldr   x29, [sp, #8]      \n"
     "   ldr   x30, [sp], #16     \n"
     "   br    x30                \n"
     "1: \n"

     :
                  "=r" (_old_this),
                  "=r" (_new_this),
     [old_sp]     "=r" (dummy1),
     [new_sp]     "=r" (dummy2)
     :
     "0" (_old_this),
     "1" (_new_this),
     "2" (&_kernel_sp),
     "3" (t->_kernel_sp)
     :  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",  "x8",
       "x11", "x12", "x13", "x14", "x15", "x16", "x17",
       "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25",
       "x26", "x27", "x28", "x30", "memory");
}
IMPLEMENT inline
void
Context::fill_user_state()
{
  // do not use 'Return_frame const *rf = regs();' here as it triggers an
  // optimization bug in gcc-4.4(.1)
  Entry_frame const *ef = regs();
  asm volatile ("msr SP_EL0, %[rf]"
                : : [rf] "r" (ef->usp));
}

IMPLEMENT inline
void
Context::spill_user_state()
{
  Entry_frame *ef = regs();
  assert (current() == this);
  asm volatile ("mrs %[rf], SP_EL0"
                : [rf] "=r" (ef->usp));
}

PRIVATE inline
void
Context::store_tpidrurw()
{
  asm volatile ("mrs %0, TPIDR_EL0" : "=r" (_tpidrurw));
}

PRIVATE inline
void
Context::load_tpidrurw() const
{
  asm volatile ("msr TPIDR_EL0, %0" : : "r" (_tpidrurw));
}

PROTECTED inline
void
Context::load_tpidruro() const
{
  asm volatile ("msr TPIDRRO_EL0, %0" : : "r" (_tpidruro));
}

IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  dst->psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
  dst->psr |= Proc::Status_mode_user | Proc::Status_always_mask;
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PUBLIC inline void Context::switch_vm_state(Context *) {}

