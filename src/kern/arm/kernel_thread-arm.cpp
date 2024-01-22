IMPLEMENTATION [arm]:

#include "config.h"
#include "cpu.h"
#include "kip_init.h"
#include "mem_space.h"

IMPLEMENT_OVERRIDE inline NEEDS["mem_space.h"]
Address
Kernel_thread::utcb_addr()
{ return Mem_space::user_max() + 1U - 0x10000U; }

IMPLEMENT inline
void
Kernel_thread::free_initcall_section()
{
  //memset(_initcall_start, 0, _initcall_end - _initcall_start);
}

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  Kip_init::map_kip(Kip::k());
  Proc::sti();
  Cpu::print_boot_infos();
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

#include "processor.h"

PROTECTED inline NEEDS["processor.h"]
void
Kernel_thread::arch_tickless_idle(unsigned)
{
  Proc::halt();
}

PROTECTED inline NEEDS["processor.h"]
void
Kernel_thread::arch_idle(unsigned)
{ Proc::halt(); }

