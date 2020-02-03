INTERFACE [libuart]:

#include "io_regblock.h"
#include "types.h"

EXTENSION class Uart
{
private:
  int _irq = -1;
};

//------------------------------------------------------
IMPLEMENTATION [libuart]:

Static_object<FIASCO_UART_TYPE> _the_uart;

IMPLEMENT
Uart::Uart() : Console(DISABLED) {}

IMPLEMENT inline
int Uart::irq() const
{
  return _irq;
}

PROTECTED bool
Uart::startup(L4::Io_register_block const *reg, int irq, Unsigned32 base_baud)
{
  _irq = irq;
  _the_uart.construct(base_baud);

  if (!_the_uart->startup(reg))
    return false;

  add_state(ENABLED);
  return true;
}

IMPLEMENT L4::Uart *
Uart::uart()
{
  return _the_uart.get();
}
