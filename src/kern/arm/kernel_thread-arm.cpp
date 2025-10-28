IMPLEMENTATION [arm]:

#include "config.h"
#include "cpu.h"
#include "kip_init.h"
#include "mem_space.h"

IMPLEMENT_OVERRIDE inline NEEDS["mem_space.h"]
Address
Kernel_thread::utcb_addr()
{ return Mem_space::max_usable_user_address() + 1U - 0x10000U; }

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
IMPLEMENTATION [amp]:

#include "amp_node.h"
#include "construction.h"
#include "kip_init.h"
#include "mem_unit.h"
#include "global_data.h"
#include "platform_control.h"

EXTENSION class Kernel_thread
{
public:
  static Global_data<bool> is_ap_node;
};

DEFINE_GLOBAL Global_data<bool> Kernel_thread::is_ap_node;

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

//--------------------------------------------------------------------------
IMPLEMENTATION [!mp && !amp]:

PUBLIC
static inline void
Kernel_thread::boot_app_cpus()
{}

