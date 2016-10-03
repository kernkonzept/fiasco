/*
 * Fiasco AMD64
 * Shared main startup/shutdown code
 */

INTERFACE[amd64]:

#include "initcalls.h"
#include "std_macros.h"

class Kernel_thread;


IMPLEMENTATION[amd64]:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "div32.h"
#include "globals.h"
#include "kernel_thread.h"
#include "kernel_task.h"

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
  static Kernel_thread *kernel = new (Ram_quota::root) Kernel_thread(Ram_quota::root);
  Task *const ktask = Kernel_task::kernel_task();
  check(kernel->bind(ktask, User<Utcb>::Ptr(0)));

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("  movq %%rax, %%cr3       \n\t"   // restore proper cr3 after running on the mp boot dir
     "	movq %3, %%rsp		\n\t"	// switch stack
     "	call call_bootstrap	\n\t"	// bootstrap kernel thread
     : "=a" (dummy), "=c" (dummy), "=d" (dummy)
     : "S" (kernel->init_stack()), "D" (kernel),
       "a" (Mem_layout::pmem_to_phys(Kmem::dir())));
}


//------------------------------------------------------------------------
IMPLEMENTATION[amd64 && mp]:

#include "kernel_thread.h"

void
main_switch_ap_cpu_stack(Kernel_thread *kernel, bool resume)
{
  Mword dummy;

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	mov %[rsp], %%rsp	\n\t"	// switch stack
     "	call call_ap_bootstrap	\n\t"	// bootstrap kernel thread
     :  "=a" (dummy), "=c" (dummy), "=d" (dummy)
     :	"D"(kernel), "S"(resume), [rsp]"r" (kernel->init_stack()));
}
