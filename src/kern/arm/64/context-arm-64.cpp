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
  register Mword _old_this asm("x1") = reinterpret_cast<Mword>(this);
  register Mword _new_this asm("x0") = reinterpret_cast<Mword>(t);
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

PROTECTED inline
void
Context::store_tpidrurw()
{
  asm volatile ("mrs %0, TPIDR_EL0" : "=r" (_tpidrurw));
  if (EXPECT_TRUE(Cpu::has_sme()))
    asm volatile ("mrs %0, S3_3_c13_c0_5" // TPIDR2_EL0, bintutils >= 2.38
                  : "=r" (_tpidr2rw));
}

PROTECTED inline
void
Context::load_tpidrurw() const
{
  asm volatile ("msr TPIDR_EL0, %0" : : "r" (_tpidrurw));
  if (EXPECT_TRUE(Cpu::has_sme()))
    asm volatile ("msr S3_3_c13_c0_5, %0" // TPIDR2_EL0, bintutils >= 2.38
                  : : "r" (_tpidr2rw));
}

PROTECTED inline
void
Context::load_tpidruro() const
{
  asm volatile ("msr TPIDRRO_EL0, %0" : : "r" (_tpidruro));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

PROTECTED inline
void
Context::sanitize_vmm_state(Return_frame *) const
{}
