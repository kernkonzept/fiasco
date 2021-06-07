IMPLEMENTATION:

#include "thread_object.h"
#include "thread_state.h"
#include "std_macros.h"

extern "C" void sys_ipc_wrapper();

IMPLEMENT void FIASCO_FLATTEN sys_ipc_wrapper()
{
  assert (!(current()->state() & Thread_drq_ready));

  Thread *curr = current_thread();
  Syscall_frame *f = curr->regs()->syscall_frame();

#ifndef NDEBUG
  if ((current()->state() & Thread_vcpu_enabled)
      && (current()->vcpu_state().access()->state & Vcpu_state::F_irqs)
      && (f->ref().have_recv() || f->tag().items() || f->tag().words()))
    WARN("VCPU makes syscall with IRQs enabled: PC=%lx\n", current()->regs()->ip());
#endif

  Obj_cap obj = f->ref();
  Utcb *utcb = curr->utcb().access(true);
  L4_fpage::Rights rights;
  Kobject_iface *o = obj.deref(&rights);
  if (EXPECT_TRUE(o!=0))
    o->invoke(obj, rights, f, utcb);
  else
    f->tag(curr->commit_error(utcb, L4_error::Not_existent));
}


//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "space.h"
#include "task.h"

extern "C" void sys_invoke_debug(Kobject_iface *o, Syscall_frame *f) __attribute__((weak));

//---------------------------------------------------------------------------
INTERFACE [ia32 || ux || amd64]:

extern void (*syscall_table[])();


//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || ux || amd64]:

void (*syscall_table[])() =
{
  sys_ipc_wrapper,
};

