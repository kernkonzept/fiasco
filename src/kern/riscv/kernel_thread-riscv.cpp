IMPLEMENTATION [riscv]:

#include "mem_layout.h"
#include "trap_state.h"

#include "warn.h"

IMPLEMENT inline
void
Kernel_thread::free_initcall_section()
{
  //memset(const_cast<char *>(Mem_layout::initcall_start), 0,
  //       Mem_layout::initcall_end - Mem_layout::initcall_start);
}

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  boot_app_cpus();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !mp]:

PRIVATE static inline
void
Kernel_thread::boot_app_cpus()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

#include "platform_control.h"

PRIVATE static inline
void
Kernel_thread::boot_app_cpus()
{
  Platform_control::boot_secondary_harts();
}
