IMPLEMENTATION [arm]:

#include "config.h"

IMPLEMENT inline
void
Kernel_thread::free_initcall_section()
{
  //memset( &_initcall_start, 0, &_initcall_end - &_initcall_start );
}

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  Proc::sti();
  boot_app_cpus();
  Proc::cli();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PUBLIC
static inline void
Kernel_thread::boot_app_cpus()
{}


//--------------------------------------------------------------------------
IMPLEMENTATION [arm && generic_tickless_idle]:

#include "mem_unit.h"
#include "processor.h"

PROTECTED inline NEEDS["processor.h", "mem_unit.h"]
void
Kernel_thread::arch_tickless_idle(unsigned)
{
  Mem_unit::tlb_flush();
  Proc::halt();
}

PROTECTED inline NEEDS["processor.h"]
void
Kernel_thread::arch_idle(unsigned)
{ Proc::halt(); }

