IMPLEMENTATION [arm]:

#include "banner.h"
#include "config.h"
#include "cpu.h"
#include "dt.h"
#include "fb_console.h"
#include "fpu.h"
#include "fpu_alloc.h"
#include "ipi.h"
#include "kernel_task.h"
#include "kernel_uart.h"
#include "kip_init.h"
#include "kmem_alloc.h"
#include "kmem_space.h"
#include "per_cpu_data.h"
#include "per_cpu_data_alloc.h"
#include "pic.h"
#include "platform_control.h"
#include "psci.h"
#include "processor.h"
#include "static_init.h"
#include "thread.h"
#include "timer.h"
#include "utcb_init.h"
#include "alternatives.h"

#include <cstdlib>
#include <cstdio>

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage1()
{
  Config::init();
  Kernel_uart::init(Kernel_uart::Init_after_mmu);
  Proc::cli();
  Cpu::early_init();
  Banner::init();
}

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage2()
{
  Cpu_number const boot_cpu = Cpu_number::boot_cpu();
  puts("Hello from Startup::stage2");
  Mem_space::init_page_sizes();

  Kip_init::init();
  Kmem_alloc::init();
  Dt::init();
  Alternative_insn::init();

  Fb_console::init(); // needs Kip_init and Kmem_alloc

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(boot_cpu);
  Per_cpu_data::run_ctors(boot_cpu);

  Kmem_space::init();
  Kernel_task::init();
  Mem_space::kernel_space(Kernel_task::kernel_task());
  Cpu::cpus.cpu(boot_cpu).init(false, true);
  Pic::init();
  Thread::init_per_cpu(boot_cpu, false);

  Platform_control::init(boot_cpu);
  Psci::init(boot_cpu);
  Fpu::init(boot_cpu, false);
  Fpu_alloc::init();
  Ipi::init(boot_cpu);
  Timer::init(boot_cpu);
  Kip_init::init_kip_clock();
  Utcb_init::init();
}
