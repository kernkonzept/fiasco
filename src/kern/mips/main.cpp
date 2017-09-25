INTERFACE [mips]:
#include <cstddef>

//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.h"
#include <construction.h>
#include "globals.h"
#include "initcalls.h"
#include "kmem_alloc.h"
#include "kip_init.h"
#include "kdb_ke.h"
#include "kernel_thread.h"
#include "kernel_task.h"
#include "kernel_console.h"
#include "reset.h"
#include "space.h"
#include "terminate.h"

#include "processor.h"

static int exit_question_active = 0;

extern "C" void __attribute__ ((noreturn))
_exit(int)
{
  if (exit_question_active)
    platform_reset();

  while (1)
    {
      Proc::halt();
      Proc::pause();
    }
}


static void exit_question()
{
  exit_question_active = 1;

  while (1)
    {
      puts("\nReturn reboots, \"k\" enters L4 kernel debugger...");

      char c = Kconsole::console()->getchar();

      if (c == 'k' || c == 'K')
        {
          kdb_ke("_exit");
        }
      else
        {
          // it may be better to not call all the destruction stuff
          // because of unresolved static destructor dependency
          // problems.
          // SO just do the reset at this point.
          puts("\033[1mRebooting...\033[0m");
          platform_reset();
          break;
        }
    }
}

extern "C" void kernel_main()
{
  static_construction();
  // caution: no stack variables in this function because we're going
  // to change the stack pointer!

  // make some basic initializations, then create and run the kernel
  // thread
  set_exit_question(&exit_question);

  // create kernel thread
  static Kernel_thread *kernel = new (Ram_quota::root) Kernel_thread;
  Task *const ktask = Kernel_task::kernel_task();
  check(kernel->bind(ktask, User<Utcb>::Ptr(0)));
  assert(((Mword)kernel->init_stack() & 7) == 0);

  register Mword a0 __asm__("a0") = reinterpret_cast<Mword>(kernel);

  // switch to stack of kernel thread and bootstrap the kernel
  __asm__ __volatile__
    ("  move $29,%0             \n"	// switch stack
     "  jal call_bootstrap      \n"
     : : "r" (kernel->init_stack()), "r" (a0));
}

//------------------------------------------------------------------------
IMPLEMENTATION[mips && mp]:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "globals.h"
#include "app_cpu_thread.h"
#include "ipi.h"
#include "mips_bsp_irqs.h"
#include "per_cpu_data_alloc.h"
#include "perf_cnt.h"
#include "pic.h"
#include "platform_control.h"
#include "spin_lock.h"
#include "timer.h"
#include "utcb_init.h"

int boot_ap_cpu() __asm__("BOOT_AP_CPU");

int boot_ap_cpu()
{
  static Cpu_number last_cpu; // keep track of the last cpu ever appeared

  Cpu_number _cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(Proc::cpu_id()));
  bool cpu_is_new = false;
  if (_cpu == Cpu_number::nil())
    {
      _cpu = ++last_cpu; // 0 is the boot cpu, so pre increment
      cpu_is_new = true;
    }

  assert (_cpu != Cpu_number::boot_cpu());

  if (cpu_is_new && !Per_cpu_data_alloc::alloc(_cpu))
    {
      extern Spin_lock<Mword> _tramp_mp_spinlock;
      printf("CPU allocation failed for CPU%u, disabling CPU.\n",
             cxx::int_value<Cpu_number>(_cpu));
      _tramp_mp_spinlock.clear();

      // FIXME: use a Platform_control API to stop the CPU
      while (1)
        Proc::halt();
    }

  if (cpu_is_new)
    Per_cpu_data::run_ctors(_cpu);

  Cpu &cpu = Cpu::cpus.cpu(_cpu);
  cpu.init(_cpu, !cpu_is_new, false);
  Platform_control::init(_cpu);
  Ipi::init(_cpu);
  Timer::init(_cpu);
  Perf_cnt::init_ap();
  Mips_bsp_irqs::init_ap(_cpu);
  Ipi::hw->init_ipis(_cpu, nullptr);

  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  void *sp = kernel->init_stack();
    {
      register Mword r0 __asm__("a0") = reinterpret_cast<Mword>(kernel);
      register Mword r1 __asm__("a1") = !cpu_is_new;

      // switch to stack of kernel thread and continue thread init
      asm volatile
        ("move $29, %0            \n"  // switch stack
         "jal  call_ap_bootstrap  \n"
         :
         : "r" (sp), "r" (r0), "r" (r1));
    }
  return 0;
}
