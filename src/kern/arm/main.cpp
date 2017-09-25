INTERFACE [arm]:
#include <cstddef>

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.h"
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

void
kernel_main()
{
  // caution: no stack variables in this function because we're going
  // to change the stack pointer!

  // make some basic initializations, then create and run the kernel
  // thread
  set_exit_question(&exit_question);

  printf("%s\n", Kip::k()->version_string());

  // disallow all interrupts before we selectively enable them
  //  pic_disable_all();

  // create kernel thread
  static Kernel_thread *kernel = new (Ram_quota::root) Kernel_thread;
  Task *const ktask = Kernel_task::kernel_task();
  check(kernel->bind(ktask, User<Utcb>::Ptr(0)));
  assert(((Mword)kernel->init_stack() & 7) == 0);

  Mem_unit::tlb_flush();

  // switch to stack of kernel thread and bootstrap the kernel
  asm volatile
    ("	mov sp, %0            \n"   // switch stack
     "	mov r0, %1            \n"   // push "this" pointer
     "	bl call_bootstrap     \n"
     : : "r" (kernel->init_stack()), "r" (kernel));
}

//------------------------------------------------------------------------
IMPLEMENTATION[arm && mp]:

#include <cstdio>
#include "config.h"
#include "cpu.h"
#include "globals.h"
#include "app_cpu_thread.h"
#include "ipi.h"
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
  cpu.init(!cpu_is_new, false);

  Pic::init_ap(_cpu, !cpu_is_new);
  Thread::init_per_cpu(_cpu, !cpu_is_new);
  Platform_control::init(_cpu);
  Ipi::init(_cpu);
  Timer::init(_cpu);
  Perf_cnt::init_ap();

  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  void *sp = kernel->init_stack();
    {
      register Mword r0 __asm__("r0") = reinterpret_cast<Mword>(kernel);
      register Mword r1 __asm__("r1") = !cpu_is_new;

      // switch to stack of kernel thread and continue thread init
      asm volatile
        ("mov sp, %0             \n"  // switch stack
         "bl  call_ap_bootstrap  \n"
         :
         : "r" (sp), "r" (r0), "r" (r1));
    }
  return 0;
}
