INTERFACE:

#include "std_macros.h"

IMPLEMENTATION:

#include <cstdio>
#include <cstdlib>
#include "helping_lock.h"
#include "kernel_console.h"
#include "reset.h"

/**
 * The exit handler as long as exit_question() is not installed.
 */
static
void
raw_exit()
{
  // make sure that we don't acknowledge the exit question automatically
  Kconsole::console()->change_state(Console::PUSH, 0,
                                    ~Console::INENABLED, 0);
  puts("\nPress any key to reboot.");
  Kconsole::console()->getchar();
  puts("\033[1mRebooting.\033[m");
  //  Cpu::busy_wait_ns(10000000);
  platform_reset();
}


static void (*exit_question)(void) = &raw_exit;

void set_exit_question(void (*eq)(void))
{
  exit_question = eq;
}


extern "C" void FIASCO_NORETURN _exit(int);

FIASCO_NORETURN
void
terminate (int exit_value)
{
  Helping_lock::threading_system_active = false;

  if (exit_question)
    exit_question();

  puts("\nShutting down...");

  _exit(exit_value);
}
