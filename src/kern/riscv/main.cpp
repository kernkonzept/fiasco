INTERFACE [riscv]:
#include <cstddef>
#include "types.h"

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "globals.h"
#include "kip_init.h"
#include "kdb_ke.h"
#include "kernel_thread.h"
#include "kernel_task.h"
#include "kernel_console.h"
#include "pic.h"
#include "processor.h"
#include "reset.h"
#include "space.h"
#include "terminate.h"

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

FIASCO_NORETURN
void
kernel_main()
{
  // caution: no stack variables in this function because we're going
  // to change the stack pointer!

  // make some basic initializations, then create and run the kernel
  // thread
  set_exit_question(&exit_question);

  Cpu::boot_cpu()->print_infos();

  // create kernel thread
  static Kernel_thread *kernel =
    new (Ram_quota::root) Kernel_thread(Ram_quota::root);
  Task *const ktask = Kernel_task::kernel_task();
  kernel->kbind(ktask);
  assert(Cpu::is_stack_aligned(reinterpret_cast<Mword>(kernel->init_stack())));

  // switch to stack of kernel thread and bootstrap the kernel
  extern char call_bootstrap[];
  Thread::riscv_fast_exit(kernel->init_stack(), &call_bootstrap, kernel);

  // never returns here
}

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

#include "app_cpu_thread.h"
#include "cpu.h"
#include "ipi.h"
#include "per_cpu_data_alloc.h"
#include "perf_cnt.h"
#include "platform_control.h"
#include "spin_lock.h"
#include "timer.h"

void boot_ap_cpu(Mword hart_id) __asm__("BOOT_AP_CPU");

void boot_ap_cpu(Mword hart_id)
{
  extern Spin_lock<Mword> _tramp_mp_spinlock;

  // Provide initial stack allocated hart context (will be replaced later on)
  Proc::Hart_context hart_context;
  hart_context._phys_id = Cpu_phys_id(hart_id);
  Proc::hart_context(&hart_context);

  Kmem::init_ap_paging();

  if (hart_id > Proc::Max_hart_id)
    {
      // Can't use panic here, as trap handling is not set up yet.
      printf("Hart ID %ld is out of allowed range, disabling hart.\n", hart_id);
      _tramp_mp_spinlock.clear();
      Platform_control::stop_cpu();
    }

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
      printf("CPU allocation failed for CPU%u, disabling CPU.\n",
             cxx::int_value<Cpu_number>(_cpu));
      _tramp_mp_spinlock.clear();
      Platform_control::stop_cpu();
    }

  if (cpu_is_new)
    Per_cpu_data::run_ctors(_cpu);

  Cpu &cpu = Cpu::cpus.cpu(_cpu);
  cpu.init(false);

  Pic::init_ap(_cpu);
  Platform_control::init(_cpu);
  Ipi::init(_cpu);
  Timer::init(_cpu);
  Perf_cnt::init_ap();

  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  void *sp = kernel->init_stack();

  register void *a0 asm("a0") = kernel;
  register Mword a1 asm("a1") = !cpu_is_new;

  // switch to stack of kernel thread and continue thread init
  __asm__ __volatile__ (
    "  mv   sp, %[stack]      \n"
    "  tail call_ap_bootstrap \n"
    :
    : [stack]   "r" (sp), "r" (a0), "r" (a1));
}
