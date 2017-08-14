IMPLEMENTATION [boston]:

// TODO: Use generic 16550_dw tag for all uart_16550_dw users

#include "koptions.h"
#include "uart_16550_dw.h"

IMPLEMENT Address Uart::base() const
{ return Koptions::o()->uart.base_address; }

IMPLEMENT int Uart::irq() const
{ return Koptions::o()->uart.irqno; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_16550_dw uart(Koptions::o()->uart.base_baud);
  return &uart;
}

IMPLEMENT
Uart::Uart() : Console(DISABLED) {}

PUBLIC bool
Uart::startup(L4::Io_register_block const *r, int, Unsigned32)
{
  if (!uart()->startup(r))
    return false;

  add_state(ENABLED);
  return true;
}
