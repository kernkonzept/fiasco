IMPLEMENTATION:

#include <cstdio>

#include "acpi.h"
#include "acpi_fadt.h"
#include "context.h"
#include "kernel_thread.h"
#include "kmem.h"
#include "pm.h"
#include "timer.h"
#include "timer_tick.h"

static bool _system_suspend_enabled;
static Unsigned32 _pm1a, _pm1b, _pm1a_sts, _pm1b_sts;
static Address phys_wake_vector;
static Acpi_facs *facs;

IMPLEMENT_OVERRIDE
void
Platform_control::init(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  Acpi_fadt const *fadt = Acpi::find<Acpi_fadt const *>("FACP");
  if (!fadt)
    {
      printf("ACPI: cannot find FADT, so suspend support disabled\n");
      return;
    }

  facs = Acpi::map_table_head<Acpi_facs>(fadt->facs_addr);
  printf("ACPI: FACS phys=%x virt=%p\n", fadt->facs_addr, facs);

  if (!facs)
    {
      printf("ACPI: cannot map FACS, so suspend support disabled\n");
      return;
    }

  if (!Acpi::check_signature(facs->signature, "FACS"))
    {
      printf("ACPI: FACS signature invalid, so suspend support disabled\n");
      return;
    }

  printf("ACPI: HW sig=%x\n", facs->hw_signature);

  extern char _tramp_acpi_wakeup[];
  phys_wake_vector = (Address)_tramp_acpi_wakeup;
  if (phys_wake_vector >= 1UL << 20)
    {
      printf("ACPI: invalid wake vector (1MB): %lx\n", phys_wake_vector);
      return;
    }

  extern volatile Address _realmode_startup_pdbr;
  _realmode_startup_pdbr = Kmem::get_realmode_startup_pdbr();
  facs->fw_wake_vector = phys_wake_vector;

  _pm1a = fadt->pm1a_cntl_blk;
  _pm1b = fadt->pm1b_cntl_blk;
  _pm1a_sts = fadt->pm1a_evt_blk;
  _pm1b_sts = fadt->pm1b_evt_blk;

  _system_suspend_enabled = true;
}



/* implemented in ia32/tramp-acpi.S */
extern "C" FIASCO_FASTCALL
int acpi_save_cpu_and_suspend(Unsigned32 sleep_type,
                              Unsigned32 pm1_cntl,
                              Unsigned32 pm1_sts);


IMPLEMENTATION [mp]:

#include "cpu_call.h"

static Cpu_mask _cpus_to_suspend;

static void
suspend_ap_cpus()
{
  // NOTE: This code must not be migrated and is not reentrant!
  _cpus_to_suspend = Cpu::online_mask();
  _cpus_to_suspend.clear(Cpu_number::boot_cpu());

  Cpu_call::cpu_call_many(_cpus_to_suspend, [](Cpu_number cpu)
    {
      Context::spill_current_fpu(cpu);
      current()->kernel_context_drq([](Context::Drq *, Context *, void *)
        {
          Cpu_number cpun = current_cpu();
          Cpu &cpu = Cpu::cpus.current();
          Pm_object::run_on_suspend_hooks(cpun);
          cpu.pm_suspend();
          check (Context::take_cpu_offline(cpun, true));
          Platform_control::prepare_cpu_suspend(cpun);
          _cpus_to_suspend.atomic_clear(current_cpu());
          Platform_control::cpu_suspend(cpun);
          return Context::Drq::no_answer_resched();
        }, 0);
      return false;
    }, true);

  while (!_cpus_to_suspend.empty())
    {
      Proc::pause();
      asm volatile ("" : "=m" (_cpus_to_suspend));
    }
}

IMPLEMENTATION [!mp]:

static void suspend_ap_cpus() {}


IMPLEMENTATION:

#include "cpu_call.h"

/**
 * \brief Initiate a full system suspend to RAM.
 * \pre must run on the boot CPU
 */
static Context::Drq::Result
do_system_suspend(Context::Drq *, Context *, void *data)
{
  assert (current_cpu() == Cpu_number::boot_cpu());
  Context::spill_current_fpu(current_cpu());
  suspend_ap_cpus();

  facs->fw_wake_vector = phys_wake_vector;
  if (facs->len > 32 && facs->version >= 1)
    facs->x_fw_wake_vector = 0;

  Mword sleep_type = *(Mword *)data;
  *reinterpret_cast<Mword*>(data) = 0;

  Pm_object::run_on_suspend_hooks(current_cpu());

  Cpu::cpus.current().pm_suspend();

  if (acpi_save_cpu_and_suspend(sleep_type,
                                (_pm1b << 16) | _pm1a,
                                (_pm1b_sts << 16) | _pm1a_sts))
    *reinterpret_cast<Mword *>(data) = -L4_err::EInval;

  Cpu::cpus.current().pm_resume();

  Pm_object::run_on_resume_hooks(current_cpu());

  Fpu::init(current_cpu(), true);

  Timer::init(current_cpu());
  Timer_tick::enable(current_cpu());
  Kernel_thread::boot_app_cpus();

  return Context::Drq::no_answer_resched();
}

IMPLEMENT_OVERRIDE
static int
Platform_control::system_suspend(Mword extra)
{
  auto guard = lock_guard(cpu_lock);

  if (!_system_suspend_enabled)
    return -L4_err::ENodev;

  Cpu_mask cpus;
  cpus.set(Cpu_number::boot_cpu());

  Cpu_call::cpu_call_many(cpus, [&extra](Cpu_number)
    {
      current()->kernel_context_drq(do_system_suspend, &extra);
      return false;
    }, true);

  return extra;
}
