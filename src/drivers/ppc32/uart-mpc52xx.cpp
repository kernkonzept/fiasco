IMPLEMENTATION[uart_mpc52xx && libuart && debug]:

#include "uart_mpc52xx.h"
#include "pic.h"
#include <boot_info.h>

IMPLEMENT Address Uart::base() const { return Boot_info::uart_base(); }

IMPLEMENT int Uart::irq() const
{ return Pic::get_irq_num((char*)"serial", (char*)"serial"); }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_mpc52xx uart;
  return &uart;
}

IMPLEMENTATION[uart_mpc52xx && libuart && !debug]:

#include "uart_mpc52xx.h"
#include <boot_info.h>

IMPLEMENT Address Uart::base() const { return Boot_info::uart_base(); }

IMPLEMENT int Uart::irq() const
{ return -1; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_mpc52xx uart;
  return &uart;
}
