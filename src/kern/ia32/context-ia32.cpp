INTERFACE [!ux]:

#include "x86desc.h"

EXTENSION class Context
{
protected:
  enum { Gdt_user_entries = 4 };
  Gdt_entry  _gdt_user_entries[Gdt_user_entries+1];
  Unsigned16 _es, _fs, _gs;
};


IMPLEMENTATION [ia32,amd64,ux]:

#include <cassert>
#include <cstdio>

#include "cpu.h"
#include "globals.h"		// current()
#include "kmem.h"
#include "lock_guard.h"
#include "space.h"
#include "thread_state.h"

PUBLIC inline
void
Context::prepare_switch_to(void (*fptr)())
{
  *reinterpret_cast<void(**)()> (--_kernel_sp) = fptr;
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
  // Set kernel-esp in case we want to return to the user.
  // kmem::kernel_sp() returns a pointer to the kernel SP (in the
  // TSS) the CPU uses when next switching from user to kernel mode.
  // regs() + 1 returns a pointer to the end of our kernel stack.
  Cpu::cpus.current().kernel_sp() = reinterpret_cast<Address>(regs() + 1);

  // switch to our page directory if necessary
  vcpu_aware_space()->switchin_context(from->vcpu_aware_space());

  // load new segment selectors
  load_segments();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || ux]:

PROTECTED inline NEEDS["cpu.h"]
void
Context::load_segments()
{
  Cpu::set_es(_es);
  Cpu::set_fs(_fs);
  Cpu::set_gs(_gs);
}

PROTECTED inline NEEDS["cpu.h"]
void
Context::store_segments()
{
  _es = Cpu::get_es();
  _fs = Cpu::get_fs();
  _gs = Cpu::get_gs();
}

//--------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include "cpu.h"
#include "gdt.h"

PROTECTED inline NEEDS ["cpu.h", "gdt.h"]
void
Context::load_gdt_user_entries(Context * /*old*/ = 0)
{
  Gdt &gdt = *Cpu::cpus.current().get_gdt();
  for (unsigned i = 0; i < Gdt_user_entries; ++i)
    gdt[(Gdt::gdt_user_entry1 / 8) + i] = _gdt_user_entries[i];

  gdt[Gdt::gdt_utcb/8] = _gdt_user_entries[Gdt_user_entries];
}


