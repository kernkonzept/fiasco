IMPLEMENTATION [bcm2835]:

#include "uart_pl011.h"
#include "mem_layout.h"

IMPLEMENT Address Uart::base() const { return Mem_layout::Uart_phys_base; }

IMPLEMENT int Uart::irq() const { return 57; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_pl011 uart(3000000);
  return &uart;
}
