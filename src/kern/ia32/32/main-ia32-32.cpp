/*
 * Fiasco ia32 / UX
 * Shared main startup/shutdown code
 */

INTERFACE[ia32,ux]:

#include "initcalls.h"
#include "std_macros.h"

class Kernel_thread;


IMPLEMENTATION[ia32,ux]:

#include <cstdio>

#include "assert_opt.h"
#include "config.h"
#include "cpu.h"
#include "div32.h"
#include "globals.h"
#include "kernel_task.h"
#include "kernel_thread.h"


FIASCO_INIT
void
kernel_main(void)
{
  unsigned dummy;

  Cpu const &cpu = *Cpu::boot_cpu();

  // caution: no stack variables in this function because we're going
  // to change the stack pointer!
  cpu.print_infos();

  printf ("\nFreeing init code/data: %lu bytes (%lu pages)\n\n",
          (Address)(&Mem_layout::initcall_end - &Mem_layout::initcall_start),
          ((Address)(&Mem_layout::initcall_end - &Mem_layout::initcall_start)
          >> Config::PAGE_SHIFT));

  // Perform architecture specific initialization
  main_arch();

  // create kernel thread
  static Kernel_thread *kernel = new (Ram_quota::root) Kernel_thread;
  assert_opt (kernel);
  Task *const ktask = Kernel_task::kernel_task();
  check(kernel->bind(ktask, User<Utcb>::Ptr(0)));

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	movl %%esi, %%esp	\n\t"	// switch stack
     "	call call_bootstrap	\n\t"	// bootstrap kernel thread
     : "=a" (dummy), "=c" (dummy), "=d" (dummy)
     : "a"(kernel), "S" (kernel->init_stack()));
}

//------------------------------------------------------------------------
IMPLEMENTATION[(ia32 || ux) && mp]:

#include "kernel_thread.h"

void
main_switch_ap_cpu_stack(Kernel_thread *kernel, bool resume)
{
  Mword dummy;

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	movl %[esp], %%esp	\n\t"	// switch stack
     "	call call_ap_bootstrap	\n\t"	// bootstrap kernel thread
     :  "=a" (dummy), "=c" (dummy), "=d" (dummy)
     :	"a"(kernel), [esp]"r" (kernel->init_stack()), "d"(resume));
}
