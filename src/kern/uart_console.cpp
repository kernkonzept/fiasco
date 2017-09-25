IMPLEMENTATION:

#include <cstdio>
#include <cstring>

#include "config.h"
#include "filter_console.h"
#include "kernel_console.h"
#include "kernel_uart.h"
#include "static_init.h"
#include "irq.h"

STATIC_INITIALIZER  (uart_console_init_stage2);

static void uart_console_init_stage2()
{
  if ((Kernel_uart::uart()->failed()))
    return;

  int irq = -1;
  if (Config::serial_esc == Config::SERIAL_ESC_IRQ
      && (irq = Kernel_uart::uart()->irq()) == -1)
    {
      puts("SERIAL ESC: not supported");
      Config::serial_esc = Config::SERIAL_ESC_NOIRQ;
    }

  switch (Config::serial_esc)
    {
    case Config::SERIAL_ESC_NOIRQ:
      puts("SERIAL ESC: No IRQ for specified uart port.");
      puts("Using serial hack in slow timer handler.");
      break;

    case Config::SERIAL_ESC_IRQ:
      Kernel_uart::enable_rcv_irq();
      printf("SERIAL ESC: allocated IRQ %d for serial uart\n", irq);
      puts("Not using serial hack in slow timer handler.");
      break;
    }
}
