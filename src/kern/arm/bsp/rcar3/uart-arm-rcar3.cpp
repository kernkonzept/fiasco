IMPLEMENTATION [arm && pf_rcar3]:

#include "koptions.h"
#include "uart_sh.h"

IMPLEMENT Address Uart::base() const
{ return Koptions::o()->uart.base_address; }

IMPLEMENT int Uart::irq() const
{ return Koptions::o()->uart.irqno; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_sh uart;
  return &uart;
}
