/*
 * Fiasco ia32
 * Main startup/shutdown code
 */
INTERFACE[ia32]:

#include "initcalls.h"
#include "std_macros.h"

class Kernel_thread;

IMPLEMENTATION[ia32]:

#include <cstdio>

#include "assert_opt.h"
#include "config.h"
#include "cpu.h"
#include "div32.h"
#include "globals.h"
#include "kernel_task.h"
#include "kernel_thread.h"
#include "paging_bits.h"

[[noreturn]] FIASCO_INIT
void
kernel_main(void)
{
  unsigned dummy;

  Cpu const &cpu = *Cpu::boot_cpu();

  // caution: no stack variables in this function because we're going
  // to change the stack pointer!
  cpu.print_infos();

  printf("\nFreeing init code/data: %zu bytes (%zu pages)\n\n",
         Mem_layout::initcall_size(), Pg::count(Mem_layout::initcall_size()));

  // Perform architecture specific initialization
  main_arch();

  // create kernel thread
  static Kernel_thread *kernel = new (Ram_quota::root) Kernel_thread(Ram_quota::root);
  assert_opt (kernel);
  Task *const ktask = Kernel_task::kernel_task();
  kernel->kbind(ktask);

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	movl %%esi, %%esp	\n\t"	// switch stack
     "	call call_bootstrap	\n\t"	// bootstrap kernel thread
     : "=a" (dummy), "=c" (dummy), "=d" (dummy)
     : "a"(kernel), "S" (kernel->init_stack())
     : "memory");

  // No return from Kernel_thread::bootstrap().
  __builtin_unreachable();
}

//------------------------------------------------------------------------
IMPLEMENTATION[ia32 && mp]:

#include "kernel_thread.h"

[[noreturn]]
void
main_switch_ap_cpu_stack(Kernel_thread *kernel, bool resume)
{
  Mword dummy;

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	movl %[esp], %%esp	\n\t"	// switch stack
     "	call call_ap_bootstrap	\n\t"	// bootstrap kernel thread
     :  "=a" (dummy), "=c" (dummy), "=d" (dummy)
     :	"a"(kernel), [esp]"r" (kernel->init_stack()), "d"(resume)
     :  "memory");

  // No return from App_cpu_thread::bootstrap().
  __builtin_unreachable();
}
