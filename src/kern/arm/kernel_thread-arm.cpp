IMPLEMENTATION [arm]:

#include "config.h"

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
  Proc::sti();
  boot_app_cpus();
  Proc::cli();
}
//--------------------------------------------------------------------------
INTERFACE [amp]:

class Kip;

EXTENSION class Kernel_thread
{
public:
  static void __amp_main(Kip *k) asm("__amp_main");
};

//--------------------------------------------------------------------------
IMPLEMENTATION [amp]:

#include "construction.h"
#include "kip_init.h"
#include "mem_unit.h"
#include "per_node_data.h"
#include "platform_control.h"

static DECLARE_PER_NODE Per_node_data<bool> is_ap_node;

IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  if (*is_ap_node)
    Platform_control::amp_ap_init();
  else
    Platform_control::amp_boot_init();
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
Kernel_thread::__amp_main(Kip *k)
{
  extern char __per_node_start[];
  extern char __per_node_end[];

  Mword id = Platform_control::node_id() - Config::First_node;
  set_amp_offset((__per_node_end - __per_node_start) * id);

  *is_ap_node = true;
  Kip::set_global_kip(k);
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

