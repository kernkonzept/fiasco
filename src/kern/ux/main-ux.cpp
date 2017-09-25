/*
 * Fiasco-UX
 * Architecture specific main startup/shutdown code
 */

IMPLEMENTATION[ux && mp]:

#include "app_cpu_thread.h"
#include "idt.h"
#include "ipi.h"
#include "per_cpu_data_alloc.h"
#include "usermode.h"

int FIASCO_FASTCALL boot_ap_cpu(Cpu_number _cpu) __asm__("BOOT_AP_CPU");

int boot_ap_cpu(Cpu_number _cpu)
{
  if (!Per_cpu_data_alloc::alloc(_cpu))
    {
      extern Spin_lock _tramp_mp_spinlock;
      printf("CPU allocation failed for CPU%u, disabling CPU.\n", _cpu);
      _tramp_mp_spinlock.clear();
      while (1)
        Proc::halt();
    }
  Per_cpu_data::run_ctors(_cpu);
  Cpu &cpu = Cpu::cpus.cpu(_cpu);

  Usermode::init(_cpu);
  Ipi::init(_cpu);

  //Kmem::init_cpu(cpu);
  Idt::load();
  //Utcb_init::init_ap(cpu);

  //Timer::init(_cpu);

  // caution: no stack variables in this function because we're going
  // to change the stack pointer!
  cpu.print();
  cpu.show_cache_tlb_info("");

  puts("");

  // create kernel thread
  App_cpu_thread *kernel = new (Ram_quota::root) App_cpu_thread();
  set_cpu_of(kernel, _cpu);
  check(kernel->bind(Kernel_task::kernel_task(), User<Utcb>::Ptr(0)));

  main_switch_ap_cpu_stack(kernel);
  return 0;
}

IMPLEMENTATION[ux]:

#include "kernel_console.h"

void main_arch()
{
  // re-enable the console -- it might have been disabled
  // by "-quiet" command line switch
  Kconsole::console()->change_state(0, 0, ~0U, Console::OUTENABLED);
}

FIASCO_INIT
int
main()
{
  kernel_main();
}
