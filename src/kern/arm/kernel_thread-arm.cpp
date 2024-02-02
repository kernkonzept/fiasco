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

//--------------------------------------------------------------------------
IMPLEMENTATION [!amp]:

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
INTERFACE [amp]:

class Kip;

EXTENSION class Kernel_thread
{
public:
  static void __amp_main() asm("__amp_main");
};

//--------------------------------------------------------------------------
IMPLEMENTATION [amp]:

#include "amp_node.h"
#include "construction.h"
#include "kip_init.h"
#include "mem_unit.h"
#include "global_data.h"
#include "platform_control.h"

static DEFINE_GLOBAL Global_data<bool> is_ap_node;

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  if (is_ap_node)
    Platform_control::amp_ap_init();
  else
    Platform_control::amp_boot_init();
  Kip_init::map_kip(Kip::k());
}

void kernel_main(void);

/**
 * Common entry point of AP CPUs.
 *
 * Will run with caches and MPU disabled! The MPU setup code will enable the
 * MPU once it is configured.
 */
IMPLEMENT
static void
Kernel_thread::__amp_main()
{
  extern char __global_data_start[];
  extern char __global_data_end[];
  size_t global_data_size = __global_data_end - __global_data_start;

  Global_data_base::set_amp_offset(global_data_size * Amp_node::id());

  is_ap_node = true;
  Platform_control::amp_ap_early_init();
  static_construction();
  kernel_main();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [!mp && !amp]:

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

