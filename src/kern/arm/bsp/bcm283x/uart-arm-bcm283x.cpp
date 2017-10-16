IMPLEMENTATION [pf_bcm283x && !pf_bcm283x_rpi3]:

#include "koptions.h"
#include "uart_pl011.h"

IMPLEMENT Address Uart::base() const
{ return Koptions::o()->uart.base_address; }

IMPLEMENT int Uart::irq() const
{ return Koptions::o()->uart.irqno; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_pl011 uart(Koptions::o()->uart.base_baud);
  return &uart;
}
