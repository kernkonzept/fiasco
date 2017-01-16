IMPLEMENTATION [sparc]:

/** Note: TCB pointer is located in sprg1 */

#include "kmem.h"
#include "utcb_support.h"

PUBLIC inline
void
Context::prepare_switch_to(void (*fptr)())
{
  _kernel_sp -= 4;
  *reinterpret_cast<void (**)()> (_kernel_sp) = fptr;
}

IMPLEMENT inline
void
Context::spill_user_state()
{}

IMPLEMENT inline
void
Context::fill_user_state()
{}

PROTECTED inline void Context::arch_setup_utcb_ptr() {}

IMPLEMENT //inline
void
Context::switch_cpu(Context *t)
{
  (void)t;
  printf("switch_cpu: %p -> %p\n", this, t);
  printf("switch_cpu: %lx -> %lx\n", (Mword)_kernel_sp, (Mword)t->_kernel_sp);
  printf("switch_cpu: %lx\n", *(Mword *)t->_kernel_sp);

  spill_user_state();

  t->fill_user_state();

  Mword dummy1, dummy2;
  register Mword _prev_this asm("o1") = (Mword)this;
  register Mword _next_this asm("o0") = (Mword)t;
  asm volatile(
      "  sub   %%sp, 16, %%sp           \n"
      "  sethi %%hi(1f - 0), %%l4       \n"
      "  or    %%l4, %%lo(1f - 0), %%l4 \n"
      "  st    %%l4, [%%sp + 0]        \n"
      "  rd    %%psr, %%l4              \n"
      "  st    %%l4, [%%sp + 4]         \n"
      "  mov   %%wim, %%l4              \n"
      "  st    %%l4, [%%sp + 8]         \n"
      "  st    %%fp, [%%sp + 12]         \n"

      "  st    %%sp, [%[prev_sp_p]]       \n"
      "  ld    [%[next_sp]], %%sp         \n"

      "  call switchin_context_label      \n"
      "   nop                             \n"
      "  ld    [%%sp + 0], %%l4           \n"
      "  jmpl  %%l4, %%g0                 \n"
      "   nop                             \n"
      "1:                                 \n"
      "  ld    [%%sp + 12], %%fp          \n"
      "  ld    [%%sp + 8], %%l4           \n"
      "  mov   %%l4, %%wim                \n"
      "  nop                              \n"
      "  nop                              \n"
      "  nop                              \n"
      "  ld    [%%sp + 4], %%l4           \n"
      "  wr    %%l4, %%psr                \n"
      "  nop                              \n"
      "  nop                              \n"
      "  nop                              \n"
      "  add   %%sp, 16, %%sp             \n"
      :
        [prev_sp_p] "=r" (dummy1),
        [next_sp]   "=r" (dummy2)
      :
        "0" (&_kernel_sp),
	"1" (&t->_kernel_sp),
	"r" (_prev_this),
	"r" (_next_this)
      :       "g1", "g2", "g3", "g4", "g5", "g6", "g7",
        "i0", "i1", "i2", "i3", "i4", "i5",       "i7",
        "l0", "l1", "l2", "l3", "l4", "l5",
                    "o2", "o3", "o4", "o5",       "o7",
        "memory");
}

/** Thread context switchin.  Called on every re-activation of a
    thread (switch_exec()).  This method is public only because it is 
    called by an ``extern "C"'' function that is called
    from assembly code (call_switchin_context).
 */

IMPLEMENT
void Context::switchin_context(Context *from)
{
  assert (this == current());
  assert (state() & Thread_ready_mask);
  printf("switchin_context\n");

  from->handle_lock_holder_preemption();

  // switch to our page directory if nessecary
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

  Utcb_support::current(this->utcb().usr());
}
