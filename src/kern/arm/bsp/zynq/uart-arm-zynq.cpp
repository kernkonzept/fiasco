IMPLEMENTATION [pf_zynq]:

#include "koptions.h"
#include "uart_cadence.h"

IMPLEMENT Address Uart::base() const
{ return Koptions::o()->uart.base_address; }

IMPLEMENT int Uart::irq() const
{ return Koptions::o()->uart.irqno; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_cadence uart(Koptions::o()->uart.base_baud);
  return &uart;
}
