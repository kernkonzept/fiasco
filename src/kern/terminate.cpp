INTERFACE:

#include "std_macros.h"
#include "types.h"

struct Exit_question
{};

IMPLEMENTATION:

#include <cstdio>
#include <cstdlib>
#include "context_base.h"
#include "global_data.h"
#include "helping_lock.h"
#include "kernel_console.h"
#include "processor.h"
#include "reset.h"
#include "static_init.h"

/**
 * The exit handler as long as exit_question() is not installed.
 */
static
void
raw_exit(Cpu_number)
{
  // make sure that we don't acknowledge the exit question automatically
  Kconsole::console()->change_state(Console::PUSH, 0,
                                    ~Console::INENABLED, 0);
  // We need to disable the UART receive interrupt to prevent the interrupt
  // handler from "stealing" the input. But actually disable all interrupts,
  // including the timer interrupt. We reboot anyway.
  Proc::cli();

  puts("\nPress any key to reboot.");
  Kconsole::console()->getchar();
  puts("\033[1mRebooting.\033[m");
  platform_reset();
}


static DEFINE_GLOBAL_PRIO(CONSTINIT_INIT_PRIO)
Global_data<void (*)(Cpu_number)> exit_question(&raw_exit);

PUBLIC static
void
Exit_question::set(void (*eq)(Cpu_number))
{
  exit_question = eq;
}

extern "C" void cov_print() __attribute__((weak));

/**
 * Terminate the kernel.
 *
 * \pre Must only be called from within a valid thread context, because
 *      current_cpu() must be functional.
 */
[[noreturn]]
void
terminate(int code)
{
  terminate_from(code, current_cpu());
}

/**
 * Terminate the kernel from a context where current_cpu() is not functional.
 *
 * \param current_cpu  Current CPU number.
 */
[[noreturn]]
void
terminate_from(int, Cpu_number current_cpu)
{
  Helping_lock::threading_system_active = false;

  // We only ask the exit question if we do not want to print coverage
  if (exit_question && !cov_print)
    exit_question(current_cpu);

  puts("\nShutting down...");

  if (cov_print)
    cov_print();

  while (1)
    {
      Proc::halt();
      Proc::pause();
    }
}

// --------------------------------------------------------------------------
IMPLEMENTATION [output && input]:

#include "kdb_ke.h"
#include "platform_control.h"
#include <cstdio>
#include <cxx/defensive>

PUBLIC static [[noreturn]]
void
Exit_question::ask(Cpu_number current_cpu)
{
  // We need to disable the UART receive interrupt to prevent the interrupt
  // handler from "stealing" the input. But actually disable all interrupts,
  // including the timer interrupt. We reboot anyway.
  Proc::cli();

  while (1)
    {
      puts("\nReturn reboots, \"k\" enters L4 kernel debugger...");

      char c = Kconsole::console()->getchar();

      if (c == 'k' || c == 'K')
        {
          kdb_ke("terminate");
        }
      else
        {
          // It may be better to not call all the destruction stuff because of
          // unresolved static destructor dependency problems. So just do the
          // reset at this point.
          puts("\033[1mRebooting...\033[0m");
          cxx::check_noreturn<Platform_control::system_reboot_from>(current_cpu);
        }
    }
}

// --------------------------------------------------------------------------
IMPLEMENTATION [!output || !input]:

#include "infinite_loop.h"
#include "platform_control.h"
#include <cxx/defensive>

PUBLIC static [[noreturn]]
void
Exit_question::ask(Cpu_number current_cpu)
{
  cxx::check_noreturn<Platform_control::system_reboot_from>(current_cpu);
}
