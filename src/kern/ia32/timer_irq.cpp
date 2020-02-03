//---------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include "config.h"
#include "globals.h"
#include "kernel_console.h"
#include "kdb_ke.h"
#include "timer.h"
#include "vkey.h"
#include "watchdog.h"


/** Slow version of timer interrupt.  Activated on every clock tick.
    Checks if something related to debugging is to do. After returning
    from this function, the real timer interrupt handler is called.
 */
extern "C"
void
thread_timer_interrupt_slow(void)
{
  if (current_cpu() == Cpu_number::boot_cpu())
    {
      if (Config::watchdog)
	{
	  // tell doggy that we are alive
	  Watchdog::touch();
	}
    }
}


