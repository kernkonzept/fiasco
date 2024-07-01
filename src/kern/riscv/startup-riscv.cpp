IMPLEMENTATION [riscv]:

#include "banner.h"
#include "config.h"
#include "cpu.h"
#include "fpu.h"
#include "ipi.h"
#include "kernel_task.h"
#include "kernel_uart.h"
#include "kip_init.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "per_cpu_data.h"
#include "per_cpu_data_alloc.h"
#include "pic.h"
#include "sbi.h"
#include "thread.h"
#include "timer.h"

#include <cstdio>


IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage1()
{
  Kernel_uart::init(Kernel_uart::Init_after_mmu);
  Proc::cli();
  Config::init();
  Banner::init();
}

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage2()
{
  printf("Hello from Startup::stage2\n");

  Sbi::init();

  Mword hart_id = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
  if (hart_id > Proc::Max_hart_id)
    panic("Hart ID %ld is out of allowed range.", hart_id);

  Kip_init::init();
  Kmem::init();
  Kmem_alloc::base_init(&Kmem::boot_map_pmem);
  Kmem_alloc::init();
  Kmem::init_paging();
  Mem_space::init_page_sizes();

  // Initialize cpu-local data management and run constructors for CPU 0
  Cpu_number const boot_cpu = Cpu_number::boot_cpu();
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(boot_cpu);
  Per_cpu_data::run_ctors(boot_cpu);

  Cpu &boot_cpu_o = Cpu::cpus.cpu(boot_cpu);
  boot_cpu_o.init(true);

  Pic::init();

  Kernel_task::init();
  Mem_space::kernel_space(Kernel_task::kernel_task());

  Fpu::init(boot_cpu, false);
  Ipi::init(boot_cpu);
  Timer::init(boot_cpu);
  Kip_init::init_kip_clock();
}
