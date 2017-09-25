IMPLEMENTATION [arm && serial]:

#include "kernel_console.h"
#include "kernel_uart.h"
#include "static_init.h"

STATIC_INITIALIZE_P(Kconsole, EARLY_INIT_PRIO);
STATIC_INITIALIZE_P(Kernel_uart, EARLY_INIT_PRIO);


