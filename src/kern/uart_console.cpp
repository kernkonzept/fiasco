IMPLEMENTATION [input]:

#include <cstdio>
#include <cstring>

#include "config.h"
#include "filter_console.h"
#include "kernel_console.h"
#include "kernel_uart.h"
#include "koptions.h"
#include "static_init.h"
#include "irq.h"

STATIC_INITIALIZER (uart_console_init_stage2);

static void uart_console_init_stage2()
{
  // Do not touch Kernel_uart::uart() if serial support is disabled as a whole.
  // The object won't be constructed in this case.
  if (Koptions::o()->opt(Koptions::F_noserial))
    return;

  if ((Kernel_uart::uart()->failed()))
    return;

  int irq = -1;
  if (Config::serial_input && (irq = Kernel_uart::uart()->irq()) == -1)
    {
      puts("SERIAL input: UART IRQ not supported.");
      Config::serial_input = Config::Serial_input_noirq;
    }

  switch (Config::serial_input)
    {
    case Config::Serial_input_noirq:
      puts("SERIAL ESC: No IRQ for specified UART port.");
      puts("Using serial hack in slow timer handler.");
      break;

    case Config::Serial_input_irq:
      Kernel_uart::enable_rcv_irq();
      printf("SERIAL ESC: allocated IRQ %d for serial UART\n", irq);
      break;
    }
}
