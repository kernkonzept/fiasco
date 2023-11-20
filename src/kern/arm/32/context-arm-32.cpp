INTERFACE [arm]:

EXTENSION class Context
{
public:
  void set_ignore_mem_op_in_progress(bool);
  bool is_ignore_mem_op_in_progress() const { return _kernel_mem_op.do_ignore; }
  bool is_kernel_mem_op_hit_and_clear();
  void set_kernel_mem_op_hit() { _kernel_mem_op.hit = 1; }

private:
  struct Kernel_mem_op
  {
    Unsigned8 do_ignore;
    Unsigned8 hit;
  };
  Kernel_mem_op _kernel_mem_op;
};

IMPLEMENTATION [arm]:

PUBLIC inline
void
Context::prepare_switch_to(void (*fptr)())
{
  *reinterpret_cast<void(**)()> (--_kernel_sp) = fptr;
}

PRIVATE inline void
Context::arm_switch_gp_regs(Context *t)
{
  register Mword _old_this asm("r1") = (Mword)this;
  register Mword _new_this asm("r0") = (Mword)t;
  register Mword _old_sp asm("r2") = (Mword)&_kernel_sp;
  register Mword _new_sp asm("r3") = (Mword)t->_kernel_sp;

  asm volatile
    (// save context of old thread
     "   stmdb sp!, {fp}          \n"
     "   adr   lr, 1f             \n"
     "   str   lr, [sp, #-4]!     \n"
     "   str   sp, [%[old_sp]]    \n"

     // switch to new stack
     "   mov   sp, %[new_sp]      \n"

     // deliver requests to new thread
     "   bl switchin_context_label \n" // call Context::switchin_context(Context *)

     // return to new context
     "   ldr   pc, [sp], #4       \n"
     "1: ldmia sp!, {fp}          \n"

     :
              "+r" (_old_this),
              "+r" (_new_this),
     [old_sp] "+r" (_old_sp),
     [new_sp] "+r" (_new_sp)
     :
     : // r11/fp is saved / restored using stmdb/ldmia
       "r4", "r5", "r6", "r7", "r8", "r9",
       "r10", "r12", "r14", "memory");
}

IMPLEMENT inline
void
Context::set_ignore_mem_op_in_progress(bool val)
{
  _kernel_mem_op.do_ignore = val;
  Mem::barrier();
}

IMPLEMENT inline
bool
Context::is_kernel_mem_op_hit_and_clear()
{
  bool h = _kernel_mem_op.hit;
  if (h)
    _kernel_mem_op.hit = 0;
  return h;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
void
Context::fill_user_state()
{
  // do not use 'Return_frame const *rf = regs();' here as it triggers an
  // optimization bug in gcc-4.4(.1)
  Entry_frame const *ef = regs();
  asm volatile ("ldmia %[rf], {sp, lr}^"
      : : "m"(ef->usp), "m"(ef->ulr), [rf] "r" (&ef->usp));
}

IMPLEMENT inline
void
Context::spill_user_state()
{
  Entry_frame *ef = regs();
  assert (current() == this);
  asm volatile ("stmia %[rf], {sp, lr}^"
      : "=m"(ef->usp), "=m"(ef->ulr) : [rf] "r" (&ef->usp));
}

PROTECTED inline
void
Context::sanitize_vmm_state(Return_frame *r) const
{
  // The continuation PSR is wrong (see Continuation::activate()) -> fix it.
  r->psr &= ~Proc::Status_mode_mask;
  r->psr |= Proc::Status_mode_user;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

PROTECTED inline
void
Context::store_tpidrurw()
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 2" : "=r" (_tpidrurw));
}

PROTECTED inline
void
Context::load_tpidrurw() const
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 2" : : "r" (_tpidrurw));
}

PROTECTED inline
void
Context::load_tpidruro() const
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 3" : : "r" (_tpidruro));
}

