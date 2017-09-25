IMPLEMENTATION [ppc32]:

#include "banner.h"
#include "boot_info.h"
#include "config.h"
#include "cpu.h"
#include "kip_init.h"
#include "kernel_task.h"
#include "kmem_alloc.h"
#include "per_cpu_data.h"
#include "per_cpu_data_alloc.h"
#include "pic.h"
#include "static_init.h"
#include "timer.h"
#include "utcb_init.h"
#include <cstdio>

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage1()
{
  Proc::cli();
  Boot_info::init();
  Cpu::early_init();
  Config::init();
}

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage2()
{
  Banner::init();
  puts("Hello from Startup::stage2");

  Kip_init::init();
  //init htab
  Mem_space::init();
  //init buddy allocator
  Kmem_alloc::init();
  //init trace buffer
  Mem_layout::init();

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();

  // not really necessary for uni processor
  Per_cpu_data_alloc::alloc(Cpu_number::boot_cpu());
  Per_cpu_data::run_ctors(Cpu_number::boot_cpu());
  Cpu::cpus.cpu(Cpu_number::boot_cpu()).init(true);

  //idle task
  Kernel_task::init();
  Pic::init();
  Timer::init(Cpu_number::boot_cpu());
  Utcb_init::init();
  puts("Startup::stage2 finished");
}

