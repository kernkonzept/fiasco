INTERFACE [sparc]:
#include <cstddef>

//---------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <cxx/defensive>

#include "config.h"
#include "globals.h"
#include "kip_init.h"
#include "kdb_ke.h"
#include "kernel_thread.h"
#include "kernel_task.h"
#include "kernel_console.h"
#include "processor.h"
#include "reset.h"
#include "space.h"
#include "terminate.h"
#include "thread_state.h"

int main()
{
  // caution: no stack variables in this function because we're going
  // to change the stack pointer!

  Exit_question::set(&Exit_question::ask);

  // create kernel thread
  Kernel_thread *kernel = Kernel_thread::create_for_boot_cpu();
  assert((reinterpret_cast<Mword>(kernel->init_stack()) & 7) == 0);

  Mem_unit::tlb_flush();

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("  mov %0, %%sp         \n"  // switch stack
     "  ba  call_bootstrap   \n"
     "   mov %1, %%o0        \n"  // push "this" pointer
     : : "r" (kernel->init_stack()), "r" (kernel) : "memory");

  // No return from Kernel_thread::bootstrap().
  __builtin_unreachable();
}
