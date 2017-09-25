IMPLEMENTATION [imx21]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx21 uart;
  return &uart;
}

IMPLEMENTATION [imx28]:

#include "koptions.h"
#include "uart_pl011.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_pl011 uart(Koptions::o()->uart.base_baud);
  return &uart;
}

IMPLEMENTATION [imx35]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx35 uart;
  return &uart;
}

IMPLEMENTATION [imx51 || imx53]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx51 uart;
  return &uart;
}

IMPLEMENTATION [imx6 || imx6ul]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx6 uart;
  return &uart;
}

IMPLEMENTATION:

#include "koptions.h"

IMPLEMENT int Uart::irq() const
{ return Koptions::o()->uart.irqno; }

IMPLEMENT Address Uart::base() const
{ return Koptions::o()->uart.base_address; }
