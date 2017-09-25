IMPLEMENTATION [uart_of && libuart]:

#include "uart_of.h"
#include "boot_info.h"

IMPLEMENT Address Uart::base() const { return Boot_info::uart_base(); }

IMPLEMENT int Uart::irq() const { return -1; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_of uart;
  return &uart;
}
