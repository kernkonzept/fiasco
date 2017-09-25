IMPLEMENTATION [serial]:

#include "kernel_console.h"
#include "kernel_uart.h"
#include "static_init.h"

STATIC_INITIALIZE_P(Kconsole, EARLY_INIT_PRIO);

static void boot_uart_init()
{
  Kernel_uart::init(Kernel_uart::Init_after_mmu);
}

STATIC_INITIALIZER_P(boot_uart_init, EARLY_INIT_PRIO);

