INTERFACE [arm]:

EXTENSION class Context
{
public:
  void set_ignore_mem_op_in_progress(bool);
  bool is_ignore_mem_op_in_progress() const { return _kernel_mem_op.do_ignore; }
  bool is_kernel_mem_op_hit_and_clear();
  void set_kernel_mem_op_hit() { _kernel_mem_op.hit = 1; }

protected:
  void sanitize_user_state(Return_frame *dst) const;

private:
  struct Kernel_mem_op
  {
    Unsigned8 do_ignore;
    Unsigned8 hit;
  };
  Kernel_mem_op _kernel_mem_op;
};

// ------------------------------------------------------------------------
INTERFACE [armv6plus]:

EXTENSION class Context
{
private:
  Mword _tpidrurw;

protected:
  Mword _tpidruro;
};


// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cassert>
#include <cstdio>

#include "globals.h"		// current()
#include "l4_types.h"
#include "cpu_lock.h"
#include "kmem.h"
#include "lock_guard.h"
#include "space.h"
#include "thread_state.h"
#include "utcb_support.h"

IMPLEMENT inline NEEDS[Context::spill_user_state, Context::store_tpidrurw,
                       Context::load_tpidrurw, Context::load_tpidruro]
void
Context::switch_cpu(Context *t)
{
  update_consumed_time();

  spill_user_state();
  store_tpidrurw();
  switch_vm_state(t);
  t->fill_user_state();
  t->load_tpidrurw();
  t->load_tpidruro();


  {
    register Mword _old_this asm("r1") = (Mword)this;
    register Mword _new_this asm("r0") = (Mword)t;
    unsigned long dummy1, dummy2;

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
                    "=r" (_old_this),
                    "=r" (_new_this),
       [old_sp]     "=r" (dummy1),
       [new_sp]     "=r" (dummy2)
       :
       "0" (_old_this),
       "1" (_new_this),
       "2" (&_kernel_sp),
       "3" (t->_kernel_sp)
       : "r4", "r5", "r6", "r7", "r8", "r9",
         "r10", "r12", "r14", "memory");
  }
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
  from->handle_lock_holder_preemption();

  // switch to our page directory if nessecary
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

  Utcb_support::current(this->utcb().usr());
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
IMPLEMENTATION [arm && !hyp]:

IMPLEMENT inline
void
Context::sanitize_user_state(Return_frame *dst) const
{
  dst->psr &= ~(Proc::Status_mode_mask | Proc::Status_interrupts_mask);
  dst->psr |= Proc::Status_mode_user | Proc::Status_always_mask;
}

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

PUBLIC inline void Context::switch_vm_state(Context *) {}

// ------------------------------------------------------------------------
IMPLEMENTATION [armv6plus]:

PROTECTED inline void Context::arch_setup_utcb_ptr()
{
  _tpidrurw = _tpidruro = reinterpret_cast<Mword>(utcb().usr().get());
}


IMPLEMENT_OVERRIDE inline
void
Context::arch_update_vcpu_state(Vcpu_state *vcpu)
{
  vcpu->host.tpidruro = _tpidruro;
}

PRIVATE inline
void
Context::store_tpidrurw()
{
  asm volatile ("mrc p15, 0, %0, c13, c0, 2" : "=r" (_tpidrurw));
}

PRIVATE inline
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

PUBLIC inline
Mword
Context::tpidrurw() const
{
  return _tpidrurw;
}

PUBLIC inline
Mword
Context::tpidruro() const
{
  return _tpidruro;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [armv6plus && !hyp]:

IMPLEMENT_OVERRIDE inline
void
Context::arch_load_vcpu_kern_state(Vcpu_state *vcpu, bool do_load)
{
  _tpidruro = vcpu->host.tpidruro;
  if (do_load)
    load_tpidruro();
}

IMPLEMENT_OVERRIDE inline
void
Context::arch_load_vcpu_user_state(Vcpu_state *vcpu, bool do_load)
{
  _tpidruro = vcpu->_regs.s.tpidruro;
  if (do_load)
    load_tpidruro();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!armv6plus]:

PROTECTED inline void Context::arch_setup_utcb_ptr()
{}

PRIVATE inline
void
Context::store_tpidrurw() const
{}

PRIVATE inline
void
Context::load_tpidrurw() const
{}

PROTECTED inline
void
Context::load_tpidruro() const
{}

PUBLIC inline
Mword
Context::tpidrurw() const
{
  return 0;
}

PUBLIC inline
Mword
Context::tpidruro() const
{
  return 0;
}

