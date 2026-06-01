INTERFACE [ppc32]:
#include <cstddef>

//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "globals.h"
//#include "kmem_alloc.h"
#include "kip_init.h"
//#include "pagetable.h"
#include "kdb_ke.h"
#include "kernel_thread.h"
#include "kernel_task.h"
#include "kernel_console.h"
//#include "reset.h" //TODO cbass: implement
#include "space.h"
//#include "terminate.h" //TODO cbass: implement

#include "processor.h"
#include "boot_info.h"
#include "thread_state.h"

int main()
{
  // caution: no stack variables in this function because we're going
  // to change the stack pointer!

  //Exit_question::set(&Exit_question::ask);

  // create kernel thread
  Kernel_thread *kernel = Kernel_thread::create_for_boot_cpu();
  //kdb_ke("init");

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile ( " mr  %%r1, %[stack]         \n" //new stack
		 " mr  %%r3, %[kernel]        \n" //"this" pointer
		 " bl call_bootstrap          \n"
		 :
		 : [stack]"r" (kernel->init_stack()),
		   [kernel]"r" (kernel)
     : "memory"
		 );

  // No return from Kernel_thread::bootstrap().
  __builtin_unreachable();
}

