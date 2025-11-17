//--------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_exynos_tickless_idle_core_offline && mp]:

#include "cpu.h"
#include "platform_control.h"
#include "processor.h"
#include "scheduler.h"

IMPLEMENT_OVERRIDE
void
Kernel_thread::arch_tickless_idle()
{
  auto cpu = current_cpu();

  if (cpu != Cpu_number::boot_cpu()
      && Platform_control::cpu_suspend_allowed(cpu))
    {
      take_cpu_offline();
      Scheduler::scheduler->trigger_hotplug_event();

      Platform_control::do_core_n_off();

      take_cpu_online();
      Scheduler::scheduler->trigger_hotplug_event();
    }
  else
    Proc::halt();
}
