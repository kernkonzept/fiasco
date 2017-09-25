IMPLEMENTATION [ppc32 && serial]:

#include "kernel_uart.h"
#include "static_init.h"

STATIC_INITIALIZER_P(boot_console_init, EARLY_INIT_PRIO);

static void boot_console_init()
{
  Console::stdout = Kernel_uart::uart();
  Console::stderr = Console::stdout;
  Console::stdin  = Console::stdout;
}

