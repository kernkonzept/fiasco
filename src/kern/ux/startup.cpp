IMPLEMENTATION:

#include "banner.h"
#include "boot_info.h"
#include "boot_alloc.h"
#include "config.h"
#include "cpu.h"
#include "fb.h"
#include "fpu.h"
#include "idt.h"
#include "initcalls.h"
#include "irq_chip_ux.h"
#include "jdb.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "kip_init.h"
#include "kmem.h"
#include "kmem_alloc.h"
#include "net.h"
#include "per_cpu_data.h"
#include "per_cpu_data_alloc.h"
#include "pic.h"
#include "static_init.h"
#include "timer.h"
#include "usermode.h"
#include "utcb_init.h"

STATIC_INITIALIZER_P(startup_system1, UX_STARTUP1_INIT_PRIO);
STATIC_INITIALIZER_P(startup_system2, STARTUP_INIT_PRIO);

static void FIASCO_INIT
startup_system1()
{
  Kconsole::init();
  Usermode::init(Cpu_number::boot_cpu());
  Boot_info::init();
  Config::init();
}

static void FIASCO_INIT
startup_system2()
{
  Banner::init();
  Kip_init::init();
  Kmem_alloc::base_init();
  Kmem_alloc::init();

  // Initialize cpu-local data management and run constructors for CPU 0
  Per_cpu_data::init_ctors();
  Per_cpu_data_alloc::alloc(Cpu_number::boot_cpu());
  Per_cpu_data::run_ctors(Cpu_number::boot_cpu());

  Mem_space::init_page_sizes();
  Kmem::init_mmu(*Cpu::boot_cpu());
  Kernel_task::init();		// enables current_mem_space()
  Kip_init::init_freq(*Cpu::boot_cpu());

  // must copy the KIP to allocated phys memory, because user apps cannot
  // access the kernel image memory
  Kip *kip = (Kip*)Kmem_alloc::allocator()->alloc(Config::PAGE_SHIFT);
  memcpy(kip, Kip::k(), Config::PAGE_SIZE);
  Kip::init_global_kip(kip);

  Utcb_init::init();
  auto irqs = new Boot_object<Irq_chip_ux>(true);
  (void)irqs; // no power management: irqs->pm_register(Cpu_number::boot_cpu());

  Ipi::init(Cpu_number::boot_cpu());
  Idt::init();
  Fpu::init(Cpu_number::boot_cpu(), false);
  Timer::init(Cpu_number::boot_cpu());
  Fb::init();
  Net::init();
  Cpu::init_global_features();
}
