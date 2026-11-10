INTERFACE:

#include "types.h"

class Platform_control
{
public:
  static void init(Cpu_number);

  /**
   * Report if this platform support shutting down CPUs.
   *
   * \retval false  CPU shutdown not supported.
   * \retval true   CPU shutdown supported.
   */
  static bool cpu_shutdown_available();

  /**
   * Allow the specified CPU to shut down.
   *
   * \param cpu    CPU which should be shut down.
   * \param allow  If `true`, shut down the CPU. If `false`, boot the CPU.
   *
   * \retval 0                Shutting down / booting was successful.
   * \retval -L4_err::ENodev  Operation not supported, either because the CPU is
   *                          still online and shall be shut down, or if the CPU
   *                          is not online but shall be booted, or because this
   *                          operation is not supported on this platform.
   */
  static int cpu_allow_shutdown(Cpu_number cpu, bool allow);

  /**
   * Initiate a system suspend.
   *
   * Blocks the calling thread until the system resumes from the suspend.
   *
   * \pre Must only be called from thread on the boot CPU, which must not be
   *      migrated to a different CPU during the suspend, otherwise the system
   *      might get stuck.
   *
   * \param extra  Platform-specific additional information to pass to the
   *               system suspend implementation.
   *
   * \return 0 on success, otherwise error code.
   */
  static int system_suspend(Mword extra);


  /**
   * Power-off the system.
   *
   * \pre Must only be called from within a valid thread context, because
   *      current_cpu() must be functional.
   *
   * \note This function returns if powering-off the system is not implemented
   *       on this platform.
   */
  static void system_off();

  /**
   * Reboot the system.
   *
   * \pre Must only be called from within a valid thread context, because
   *      current_cpu() must be functional.
   */
  [[noreturn]] static void system_reboot();

  /**
   * Reboot the system from a context where current_cpu() is not functional.
   *
   * \param current_cpu  Current CPU number.
   */
  [[noreturn]] static void system_reboot_from(Cpu_number current_cpu);
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "cpu_lock.h"
#include "l4_types.h"
#include "lock_guard.h"
#include "pm.h"
#include "reset.h"

IMPLEMENT_DEFAULT inline
void
Platform_control::init(Cpu_number)
{}

IMPLEMENT_DEFAULT inline NEEDS["l4_types.h"]
int
Platform_control::system_suspend(Mword)
{ return -L4_err::EBusy; }

IMPLEMENT_DEFAULT inline
bool
Platform_control::cpu_shutdown_available()
{ return false; }

IMPLEMENT_DEFAULT inline NEEDS["l4_types.h"]
int
Platform_control::cpu_allow_shutdown(Cpu_number, bool)
{ return -L4_err::ENodev; }

IMPLEMENT_DEFAULT inline NEEDS["pm.h"]
void
Platform_control::system_off()
{
  // As there is actually no shutdown, do NOT execute the shutdown hooks!
}

IMPLEMENT [[noreturn]] inline NEEDS["cpu_lock.h", "lock_guard.h"]
void
Platform_control::system_reboot()
{
  auto guard = lock_guard(cpu_lock);
  system_reboot_from(current_cpu());
}

IMPLEMENT_DEFAULT [[noreturn]] inline
NEEDS["cpu_lock.h", "lock_guard.h", "pm.h", "reset.h"]
void
Platform_control::system_reboot_from(Cpu_number current_cpu)
{
  auto guard = lock_guard(cpu_lock);

  Cpu_pm_callbacks::run_on_shutdown_hooks(current_cpu);
  platform_reset();
}
