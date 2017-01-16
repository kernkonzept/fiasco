IMPLEMENTATION [pf_imx_21]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx21 uart;
  return &uart;
}

IMPLEMENTATION [pf_imx_28]:

#include "koptions.h"
#include "uart_pl011.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_pl011 uart(Koptions::o()->uart.base_baud);
  return &uart;
}

IMPLEMENTATION [pf_imx_35]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx35 uart;
  return &uart;
}

IMPLEMENTATION [pf_imx_51 || pf_imx_53]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx51 uart;
  return &uart;
}

IMPLEMENTATION [pf_imx_6 || pf_imx_6ul]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx6 uart;
  return &uart;
}

IMPLEMENTATION [pf_imx_7]:

#include "uart_imx.h"

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_imx7 uart;
  return &uart;
}

IMPLEMENTATION:

#include "koptions.h"

IMPLEMENT int Uart::irq() const
{ return Koptions::o()->uart.irqno; }

IMPLEMENT Address Uart::base() const
{ return Koptions::o()->uart.base_address; }
