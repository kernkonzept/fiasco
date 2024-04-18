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
  register Mword _old_this asm("r1") = reinterpret_cast<Mword>(this);
  register Mword _new_this asm("r0") = reinterpret_cast<Mword>(t);
  register Mword _old_sp asm("r2") = reinterpret_cast<Mword>(&_kernel_sp);
  register Mword _new_sp asm("r3") = reinterpret_cast<Mword>(t->_kernel_sp);

  asm volatile
    (// save context of old thread
#ifdef __thumb__
     "   stmdb sp!, {r7}          \n" // r7 frame pointer in thumb mode
     "   adr   lr, (1f + 1)       \n" // make sure to return to thumb mode
#else
     "   stmdb sp!, {fp}          \n" // r11 frame pointer in ARM mode
     "   adr   lr, 1f             \n"
#endif
     "   str   lr, [sp, #-4]!     \n"
     "   str   sp, [%[old_sp]]    \n"

     // switch to new stack
     "   mov   sp, %[new_sp]      \n"

     // deliver requests to new thread
     "   bl switchin_context_label \n" // call Context::switchin_context(Context *)

     // return to new context
     "   ldr   pc, [sp], #4       \n"
     "1:                          \n"
#ifdef __thumb__
     "   ldmia sp!, {r7}          \n"
#else
     "   ldmia sp!, {fp}          \n"
#endif

     :
              "+r" (_old_this),
              "+r" (_new_this),
     [old_sp] "+r" (_old_sp),
     [new_sp] "+r" (_new_sp)
     :
     : // THUMB2: r7 frame pointer and saved/restored using stmdb/ldmia
       // ARM: r11 frame pointer and saved/restored using stmdb/ldmia
       "r4", "r5", "r6", "r8", "r9", "r10", "r12", "r14",
#ifdef __thumb__
       "r11",
#else
       "r7",
#endif
       "memory");
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
IMPLEMENTATION [arm && !cpu_virt && !thumb2]:

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

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt && thumb2]:

/*
 * Unfortunately, the access to the banked user mode stack pointer and link
 * register must be done in arm mode. Put the assembly into dedicated functions
 * that are forced to be arm code and that *cannot* be inlined.
 *
 * There are thumb mode equivalents available but only starting with armv7ve.
 */

EXTENSION class Context
{
  static void __attribute__((target("arm"),noinline))
  fill_user_state_arm(Entry_frame const *ef);

  static void __attribute__((target("arm"),noinline))
  spill_user_state_arm(Entry_frame *ef);
};

IMPLEMENT
void
Context::fill_user_state_arm(Entry_frame const *ef)
{
  asm volatile ("ldmia %[rf], {sp, lr}^"
                : : "m"(ef->usp), "m"(ef->ulr), [rf] "r" (&ef->usp));
}

IMPLEMENT inline
void
Context::fill_user_state()
{ fill_user_state_arm(regs()); }

IMPLEMENT
void
Context::spill_user_state_arm(Entry_frame *ef)
{
  asm volatile ("stmia %[rf], {sp, lr}^"
                : "=m"(ef->usp), "=m"(ef->ulr) : [rf] "r" (&ef->usp));
}

IMPLEMENT inline
void
Context::spill_user_state()
{
  assert (current() == this);
  spill_user_state_arm(regs());
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

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

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu]:

IMPLEMENT_OVERRIDE inline
void
Context::save_fpu_state_to_utcb(Trap_state *ts, Utcb *u)
{
  auto *esu = reinterpret_cast<Fpu::Exception_state_user *>(&u->values[21]);
  Fpu::save_user_exception_state(state() & Thread_fpu_owner, fpu_state().get(),
                                 ts, esu);
}
