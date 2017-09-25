IMPLEMENTATION [ppc32]:

/** Note: TCB pointer is located in sprg1 */

/*
#include <cassert>
#include <cstdio>

#include "l4_types.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "space.h"
#include "thread_state.h"
*/

#include "kmem.h"
#include "utcb_support.h"

IMPLEMENT inline
void
Context::spill_user_state()
{}

IMPLEMENT inline
void
Context::fill_user_state()
{}

PROTECTED inline void Context::arch_setup_utcb_ptr() {}

IMPLEMENT inline
void
Context::switch_cpu(Context *t)
{
  unsigned long dummy1, dummy2, dummy3;

//  printf("Switch %p -> %p (sp %p -> %p)\n", this, t, _kernel_sp, t->_kernel_sp);
//  WARNING: switchin_context_label needs a Context* parameter (old Context *)
  asm volatile (" lis    %%r5, 1f@ha                \n"
		" addi   %%r5, %%r5, 1f@l           \n"

		//save return addr
		" stw    %%r5, 0(%%r1)              \n"
		" stw    %%r1, 0(%[kernel_sp])      \n"

		//switch stack
		" mr     %%r1, %[new_sp]            \n"
		" mr     %%r3, %[new_thread]        \n"

		//address space switch
		" bl     switchin_context_label     \n"
		" lwz    %%r5, 0(%%r1)              \n"
		" mtctr  %%r5                       \n"
		" bctr                              \n"
		"1:                                 \n"
		: [new_thread]"=r" (dummy1),
		  [kernel_sp] "=r" (dummy2),
		  [new_sp]    "=r" (dummy3)
		: "0" (t),
		  "1" (&_kernel_sp),
		  "2" (t->_kernel_sp)
		: "r0",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
		  "r8",  "r9",  "r13", "r14", "r15", "r16", "r17", 
		  "r18", "r19", "r20", "r21", "r22", "r23", "r24",
		  "r25", "r26", "r27", "r28", "r29", "r30", "r31",
		  "ctr", "lr",  "cr2", "cr3", "cr4", "xer", "memory"
		  );
}

/** Thread context switchin.  Called on every re-activation of a
    thread (switch_exec()).  This method is public only because it is 
    called by an ``extern "C"'' function that is called
    from assembly code (call_switchin_context).
 */

IMPLEMENT
void Context::switchin_context(Context *from)
{
  assert(this == current());
  assert(state() & Thread_ready);
  // Set kernel-esp in case we want to return to the user.
  // kmem::kernel_esp() returns a pointer to the kernel SP (in the
  // TSS) the CPU uses when next switching from user to kernel mode.  
  // regs() + 1 returns a pointer to the end of our kernel stack.
  Kmem::kernel_sp( reinterpret_cast<Mword*>(regs() + 1) );
#if 0
  printf("switch in address space: %p\n",_space);
#endif

  // switch to our page directory if nessecary
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

  Utcb_support::current(utcb().usr());
}
