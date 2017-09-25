IMPLEMENTATION [sparc]:

#include "mem_layout.h"
#include "psr.h"
IMPLEMENT inline
void
Kernel_thread::free_initcall_section()
{
  memset( (void*)&Mem_layout::initcall_start, 0, &Mem_layout::initcall_end
          - &Mem_layout::initcall_start );
  printf("%d KB kernel memory freed\n", (int)(&Mem_layout::initcall_end -
         &Mem_layout::initcall_start)/1024);
}

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  //status for kernel thread
  //Return_frame *rf = nonull_static_cast<Return_frame*>(current()->regs());
  //rf->srr1 = Msr::Msr_kernel;
}
