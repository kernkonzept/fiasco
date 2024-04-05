IMPLEMENTATION:

#include <cstdio>

#include "acpi.h"
#include "acpi_fadt.h"
#include "context.h"
#include "kmem.h"
#include "pm.h"
#include "reset.h"

static bool _system_suspend_enabled = false;
static void (*_system_resume_handler)() = nullptr;

// Ensures that only one system suspend is done at a time, effectively a bool,
// but stored as an Mword since accessed with cas.
static Mword _system_suspend_pending;

// Values cached from ACPI FADT, initialized in Platform_control::init
static Unsigned32 _pm1a, _pm1b, _pm1a_sts, _pm1b_sts;
static Unsigned32 _fadt_reset_value, _fadt_reset_regs_addr;
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
  printf("ACPI: FACS phys=%x virt=%p\n", fadt->facs_addr,
         static_cast<void *>(facs));

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
  phys_wake_vector = reinterpret_cast<Address>(_tramp_acpi_wakeup);
  if (phys_wake_vector >= 1UL << 20)
    {
      printf("ACPI: invalid wake vector (1MB): %lx\n", phys_wake_vector);
      return;
    }

  extern volatile Address _realmode_startup_pdbr;
  _realmode_startup_pdbr = Kmem::get_realmode_startup_pdbr();
  facs->fw_wake_vector = phys_wake_vector;

  // The fadt pointer is only valid in the kernel address space of idle. To
  // avoid dereferencing it in a different context, we cache the values we
  // need.
  _pm1a = fadt->pm1a_cntl_blk;
  _pm1b = fadt->pm1b_cntl_blk;
  _pm1a_sts = fadt->pm1a_evt_blk;
  _pm1b_sts = fadt->pm1b_evt_blk;
  _fadt_reset_value = fadt->reset_value;
  _fadt_reset_regs_addr = fadt->reset_regs.addr;

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

/**
 * Suspend all AP CPUs and take the boot CPU offline.
 *
 * \note Must be executed on the boot CPU, i.e. the calling thread must be and
 *       stay on the boot CPU.
 * \note Releases the CPU lock temporarily to make cross-CPU call to AP CPUs.
 */
static void
suspend_ap_cpus()
{
  // NOTE: This code must not be migrated and is not reentrant!
  _cpus_to_suspend = Cpu::online_mask();
  _cpus_to_suspend.clear(Cpu_number::boot_cpu());

  auto suspend_ap_cpu = [](Cpu_number cpu)
    {
      Context::spill_current_fpu(cpu);
      current()->kernel_context_drq([](Context::Drq *, Context *, void *)
        {
          Cpu_number cpun = current_cpu();
          Cpu &cpu = Cpu::cpus.current();
          Pm_object::run_on_suspend_hooks(cpun);
          cpu.pm_suspend();
          check (Context::take_cpu_offline(cpun, true));
          // We assume that Platform_control::cpu_suspend() does never return
          // under any circumstances -- otherwise we'd run with inconsistent
          // state into the scheduler.
          Sched_context::rq.cpu(cpun).schedule_in_progress = 0;
          Platform_control::prepare_cpu_suspend(cpun);
          _cpus_to_suspend.atomic_clear(current_cpu());
          Platform_control::cpu_suspend(cpun);
          Mem_unit::tlb_flush();
          return Context::Drq::no_answer_resched();
        }, 0);
      return false;
    };

    {
      // If the thread is migrated here, the suspend will not finish.
      auto guard = lock_guard<Lock_guard_inverse_policy>(cpu_lock);
      Cpu_call::cpu_call_many(_cpus_to_suspend, suspend_ap_cpu, true);
    }

  // Wind up pending Rcu and Drq changes together with all _cpus_to_suspend
  check (Context::take_cpu_offline(current_cpu(), true));

  while (!_cpus_to_suspend.empty())
    {
      Proc::pause();
      asm volatile ("" : "=m" (_cpus_to_suspend));
    }
}

static void
take_boot_cpu_online()
{
  Context::take_cpu_online(current_cpu());
}

IMPLEMENTATION [!mp]:

static void suspend_ap_cpus() {}
static void take_boot_cpu_online() {}

IMPLEMENTATION:

#include "io.h"
#include "thread_state.h"

PUBLIC static
void
Platform_control::set_system_resume_handler(void (*system_resume_handler)())
{
  _system_resume_handler = system_resume_handler;
}

/**
 * Initiate a full system suspend to RAM.
 *
 * \pre CPU lock must be held.
 *
 * \note Must be executed on the boot CPU, i.e. the calling thread must be and
 *       stay on the boot CPU.
 */
static Mword
do_system_suspend(Mword sleep_type)
{
  assert(cpu_lock.test());
  assert(current_cpu() == Cpu_number::boot_cpu());

  Mword result = 0;

  // First suspend all the other CPUs in the system (temporarily releases CPU
  // lock).
  suspend_ap_cpus();

  // Then enter system suspend via ACPI.
  Context::spill_current_fpu(current_cpu());

  facs->fw_wake_vector = phys_wake_vector;
  if (facs->len > 32 && facs->version >= 1)
    facs->x_fw_wake_vector = 0;

  Pm_object::run_on_suspend_hooks(current_cpu());

  current()->spill_user_state();

  Cpu::cpus.current().pm_suspend();

  if (acpi_save_cpu_and_suspend(sleep_type,
                                (_pm1b << 16) | _pm1a,
                                (_pm1b_sts << 16) | _pm1a_sts))
    result = -L4_err::EInval;

  Mem_unit::tlb_flush();

  Cpu::cpus.current().pm_resume();

  // mainly for setting FS base and GS base on AMD64
  // must be done after calling Cpu::pm_resume()
  current()->fill_user_state();

  take_boot_cpu_online();

  Pm_object::run_on_resume_hooks(current_cpu());

  // includes booting the application CPUs
  if (_system_resume_handler)
    _system_resume_handler();

  return result;
}

IMPLEMENT_OVERRIDE
static int
Platform_control::system_suspend(Mword extra)
{
  if (!_system_suspend_enabled)
    return -L4_err::ENodev;

  auto guard = lock_guard(cpu_lock);

  // Must only be invoked from boot CPU and must not be migrated.
  if (   current()->home_cpu() != Cpu_number::boot_cpu()
      || current_cpu() != Cpu_number::boot_cpu())
    return -L4_err::EInval;

  // Mark systems suspend as pending, fails if already in progress.
  if (!cas(&_system_suspend_pending, Mword{false}, Mword{true}))
    return -L4_err::EBusy;

  Mem::mp_mb();
  Mword result = do_system_suspend(extra);
  Mem::mp_mb();

  write_now(&_system_suspend_pending, false);

  return result;
}

IMPLEMENT_OVERRIDE
static void
Platform_control::system_reboot()
{
  auto guard = lock_guard(cpu_lock);

  if (!_system_suspend_enabled)
    return;

  Io::out8(_fadt_reset_value, _fadt_reset_regs_addr);

  // if ACPI reset failed, try other methods
  platform_reset();
}
