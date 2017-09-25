INTERFACE [16550]:

#include "io_regblock.h"

EXTENSION class Uart
{
public:
  int _irq;
};


IMPLEMENTATION [16550]:

#include "uart_16550.h"

Static_object<L4::Uart_16550> _the_uart;

IMPLEMENT
Uart::Uart() : Console(DISABLED) {}

PUBLIC bool
Uart::startup(const L4::Io_register_block *r, int irq,
              Unsigned32 base_baud)
{
  _irq = irq;
  _the_uart.construct(base_baud, L4::Uart_16550::F_skip_init);
  if (!_the_uart->startup(r))
    return false;

  add_state(ENABLED);
  return true;
}

IMPLEMENT inline
int Uart::irq() const
{
  return _irq;
}

IMPLEMENT L4::Uart *Uart::uart()
{
  return _the_uart.get();
}


