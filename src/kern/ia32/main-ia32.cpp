/*
 * Fiasco-IA32/AMD64
 * Architecture specific main startup/shutdown code
 */

IMPLEMENTATION[ia32,amd64]:

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "config.h"
#include "io.h"
#include "idt.h"
#include "kdb_ke.h"
#include "kernel_console.h"
#include "koptions.h"
#include "pic.h"
#include "processor.h"
#include "reset.h"
#include "timer.h"
#include "timer_tick.h"
#include "terminate.h"

static int exit_question_active;


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


static
void
exit_question()
{
  Proc::cli();
  exit_question_active = 1;

  Unsigned16 irqs = Pic::disable_all_save();
  if (Config::getchar_does_hlt_works_ok)
    {
      Timer_tick::set_vectors_stop();
      Timer_tick::enable(Cpu_number::boot_cpu()); // hm, exit always on CPU 0
      Proc::sti();
    }

  // make sure that we don't acknowledge the exit question automatically
  Kconsole::console()->change_state(Console::PUSH, 0, ~Console::INENABLED, 0);
  puts("\nReturn reboots, \"k\" enters L4 kernel debugger...");

  char c = Kconsole::console()->getchar();

  if (c == 'k' || c == 'K') 
    {
      Pic::restore_all(irqs);
      kdb_ke("_exit");
    }
  else
    {
      // It may be better to not call all the destruction stuff because of
      // unresolved static destructor dependency problems. So just do the
      // reset at this point.
      puts("\033[1mRebooting.\033[m");
    }
}

void
main_arch()
{
  // console initialization
  set_exit_question(&exit_question);
}


//------------------------------------------------------------------------
IMPLEMENTATION[(ia32,amd64) && mp]:

#include <cstdio>
#include "apic.h"
#include "app_cpu_thread.h"
#include "config.h"
#include "cpu.h"
#include "div32.h"
#include "fpu.h"
#include "globals.h"
#include "ipi.h"
#include "kernel_task.h"
#include "processor.h"
#include "per_cpu_data_alloc.h"
#include "perf_cnt.h"
#include "platform_control.h"
#include "spin_lock.h"
#include "utcb_init.h"

int FIASCO_FASTCALL boot_ap_cpu() __asm__("BOOT_AP_CPU");

static void FIASCO_NORETURN
stop_booting_ap_cpu(char const *msg, Unsigned32 apic_id)
{
  extern Spin_lock<Mword> _tramp_mp_spinlock;
  printf("%s, disabling CPU: %x\n", msg, apic_id);
  _tramp_mp_spinlock.clear();

  while (1)
    Proc::halt();
}

int FIASCO_FASTCALL boot_ap_cpu()
{
  Apic::activate_by_msr();

  Unsigned32 apic_id = Apic::get_id();
  Cpu_number _cpu = Apic::find_cpu(apic_id);
  bool cpu_is_new = false;

  // keep track of the last cpu ever appeared
  static Cpu_number last_cpu = Cpu_number::first();
  if (_cpu == Cpu_number::nil())
    {
      if (Kernel_thread::boot_deterministic)
        {
          _cpu = Kernel_thread::find_cpu_num_by_apic_id(apic_id);
          if (Cpu_number::nil() == _cpu)
            stop_booting_ap_cpu("Previously unknown CPU", apic_id);
        }
      else
        _cpu = ++last_cpu;

      cpu_is_new = true;
    }

  if (cpu_is_new && !Per_cpu_data_alloc::alloc(_cpu))
    stop_booting_ap_cpu("CPU allocation failed", apic_id);

  if (cpu_is_new)
    Per_cpu_data::run_ctors(_cpu);

  Cpu &cpu = Cpu::cpus.cpu(_cpu);


  if (cpu_is_new)
    {
      Kmem::init_cpu(cpu);
      Idt::init_current_cpu();
      Apic::init_ap();
      Apic::apic.cpu(_cpu).construct(_cpu);
      Ipi::init(_cpu);
    }
  else
    {
      Kmem::resume_cpu(_cpu);
      Idt::load();
      cpu.pm_resume();
      Pm_object::run_on_resume_hooks(_cpu);
    }

  Timer::init(_cpu);

  if (cpu_is_new)
    Platform_control::init(_cpu);

  if (Koptions::o()->opt(Koptions::F_loadcnt))
    Perf_cnt::init_ap();


  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  main_switch_ap_cpu_stack(kernel, !cpu_is_new);
  return 0;
}
