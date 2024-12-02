INTERFACE [arm]:

#include "types.h"

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstdlib>
#include <cstdio>
#include <construction.h>

[[noreturn]] void kernel_main(void);

extern "C"
[[noreturn]] void __main()
{
  static_construction();
  kernel_main();
}

//------------------------------------------------------------------------
IMPLEMENTATION[arm && amp]:

#include "global_data.h"
#include "kernel_thread.h"
#include "platform_control.h"

[[noreturn]] void __amp_main() __asm__("__amp_main");

[[noreturn]] void
__amp_main()
{
  extern char __global_data_start[];
  extern char __global_data_end[];
  size_t global_data_size = __global_data_end - __global_data_start;

  Global_data_base::set_amp_offset(global_data_size * Amp_node::id());

  Kernel_thread::is_ap_node = true;
  Platform_control::amp_ap_early_init();
  static_construction();
  kernel_main();
}
