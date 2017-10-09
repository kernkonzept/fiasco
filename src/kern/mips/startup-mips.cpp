/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Prajna Dasgupta <prajna@kymasys.com>
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 *
 * (2015)
 * Author: Alexander Warg <alexander.warg@kernkonzept.com>
 */

IMPLEMENTATION [mips]:

#include "banner.h"
#include "cm.h"
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
#include "perf_cnt.h"
#include "platform_control.h"
#include "mips_cpu_irqs.h"
#include "mips_bsp_irqs.h"
#include "static_init.h"
#include "thread.h"
#include "timer.h"

#include <cstdio>

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage1()
{
  Proc::cli();
  Banner::init();
  Config::init();
}

IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void
Startup::stage2()
{
  printf("Hello from Startup::stage2\n");

  if (Config::Max_num_cpus > 1)
    {
      printf("Max_num_cpus %d\n", Config::Max_num_cpus);
    }

  Cpu_number const boot_cpu = Cpu_number::boot_cpu();
  Cpu &boot_cpu_o = Cpu::cpus.cpu(boot_cpu);

  boot_cpu_o.init(Cpu_number::boot_cpu(), false, true);
  Mem_unit::init_tlb();

  //Mem_op::cache_init();
  Kip_init::init();
  Mem_space::init();
  Kmem_alloc::init();

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(boot_cpu);
  Per_cpu_data::run_ctors(boot_cpu);

  Kernel_task::init();

  Mem_space::kernel_space(Kernel_task::kernel_task());

  Cm::init();
  Mem_unit::cache_detect(Cm::cm ? Cm::cm->l2_cache_line() : 0);
  Mips_cpu_irqs::init();
  Mips_bsp_irqs::init(boot_cpu);

  Kip::k()->frequency_cpu = boot_cpu_o.frequency() / 1000;

  Platform_control::init(boot_cpu);
  Fpu::init(boot_cpu, false);
  Ipi::init(boot_cpu);
  //Thread_ipi::init(boot_cpu);
  Timer::init(boot_cpu);
  Perf_cnt::init_ap();
}

