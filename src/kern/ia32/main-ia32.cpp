/*
 * Fiasco-IA32/AMD64
 * Architecture specific main startup/shutdown code
 */

IMPLEMENTATION[ia32 || amd64]:

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
#include "platform_control.h"
#include "processor.h"
#include "reset.h"
#include "timer.h"
#include "timer_tick.h"
#include "terminate.h"

static int exit_question_active;


extern "C" [[noreturn]] void
_exit(int)
{
  if (exit_question_active)
    Platform_control::system_reboot();

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
IMPLEMENTATION[(ia32 || amd64) && mp]:

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

[[noreturn]] void FIASCO_FASTCALL boot_ap_cpu() __asm__("BOOT_AP_CPU");

[[noreturn]] static void
stop_booting_ap_cpu(char const *msg, Apic_id apic_id)
{
  extern Spin_lock<Mword> _tramp_mp_spinlock;
  printf("%s, disabling CPU: %x\n", msg, cxx::int_value<Apic_id>(apic_id));
  _tramp_mp_spinlock.clear();

  while (1)
    Proc::halt();
}

[[noreturn]] void FIASCO_FASTCALL boot_ap_cpu()
{
  Apic::activate_by_msr();

  Apic_id apic_id = Apic::get_id();
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

  // the CPU feature flags may have changed after activating the Apic
  cpu.update_features_info();

  if (cpu_is_new)
    {
      Kmem::init_cpu(cpu);
      Apic::apic.cpu(_cpu).construct(_cpu); // do before IDT setup!
      Idt::init_current_cpu();
      Apic::init_ap();
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
  Timer::init_system_clock_ap(_cpu);

  if (cpu_is_new)
    Platform_control::init(_cpu);

  Perf_cnt::init_ap(cpu);

  // create kernel thread
  Kernel_thread *kernel = App_cpu_thread::may_be_create(_cpu, cpu_is_new);

  main_switch_ap_cpu_stack(kernel, !cpu_is_new);
}
