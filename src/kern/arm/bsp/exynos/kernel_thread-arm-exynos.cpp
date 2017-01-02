//--------------------------------------------------------------------------
IMPLEMENTATION [arm && tickless_idle && pf_exynos && mp]:

#include "cpu.h"
#include "platform_control.h"
#include "processor.h"
#include "scheduler.h"

PROTECTED inline NEEDS["processor.h", "cpu.h", "platform_control.h", "scheduler.h"]
void
Kernel_thread::arch_tickless_idle(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu() && Platform_control::cpu_suspend_allowed(cpu))
    {
      take_cpu_offline(cpu);
      Scheduler::scheduler.trigger_hotplug_event();

      Platform_control::do_core_n_off(cpu);

      take_cpu_online(cpu);
      Scheduler::scheduler.trigger_hotplug_event();
    }
  else
    Proc::halt();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && tickless_idle && pf_exynos && !mp]:

#include "cpu.h"
#include "processor.h"

PROTECTED inline NEEDS["processor.h", "cpu.h"]
void
Kernel_thread::arch_tickless_idle(Cpu_number)
{
  Proc::halt();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && tickless_idle && pf_exynos]:

PROTECTED inline NEEDS["processor.h"]
void
Kernel_thread::arch_idle(Cpu_number)
{
  Proc::halt();
}
