IMPLEMENTATION [arm]:

#include "config.h"
#include "cpu.h"
#include "fpu.h"
#include "ipi.h"
#include "kernel_task.h"
#include "kernel_uart.h"
#include "kip_init.h"
#include "kmem_alloc.h"
#include "kmem_space.h"
#include "per_cpu_data.h"
#include "per_cpu_data_alloc.h"
#include "perf_cnt.h"
#include "pic.h"
#include "platform_control.h"
#include "processor.h"
#include "static_init.h"
#include "thread.h"
#include "timer.h"
#include "utcb_init.h"

#include <cstdlib>
#include <cstdio>

//------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && !cpu_virt]:

#include "static_init.h"

static void add_initial_pmem()
{
    // The first 4MB of phys memory are always mapped to Map_base
  Mem_layout::add_pmem(Mem_layout::Sdram_phys_base, Mem_layout::Map_base,
                       4 << 20);
}

STATIC_INITIALIZER_P(add_initial_pmem, 101);

//------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage1()
{
  Kernel_uart::init(Kernel_uart::Init_after_mmu);
  Proc::cli();
  Cpu::early_init();
  Config::init();
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

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(boot_cpu);
  Per_cpu_data::run_ctors(boot_cpu);

  Kmem_space::init();
  Kernel_task::init();
  Mem_space::kernel_space(Kernel_task::kernel_task());
  Pic::init();
  Thread::init_per_cpu(boot_cpu, false);

  Cpu::init_mmu(true);
  Cpu::cpus.cpu(boot_cpu).init(false, true);
  Platform_control::init(boot_cpu);
  Fpu::init(boot_cpu, false);
  Ipi::init(boot_cpu);
  Timer::init(boot_cpu);
  Utcb_init::init();
}
