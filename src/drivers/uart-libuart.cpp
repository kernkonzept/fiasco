INTERFACE [libuart]:

#include "io_regblock.h"
#include "types.h"

EXTENSION class Uart
{
private:
  static void init_uart_instance(Unsigned32 base_baud);
  void bsp_init();

  int _irq = -1;
  static L4::Uart *_uart;
};

//------------------------------------------------------
IMPLEMENTATION [libuart]:

Static_object<FIASCO_UART_TYPE> _the_uart;

L4::Uart *Uart::_uart;

IMPLEMENT
Uart::Uart() : Console(DISABLED) {}

IMPLEMENT inline
int Uart::irq() const
{
  return _irq;
}

IMPLEMENT_DEFAULT
void
Uart::bsp_init()
{}

PROTECTED bool
Uart::startup(L4::Io_register_block const *reg, int irq, Unsigned32 base_baud,
              bool resume)
{
  _irq = irq;

  bsp_init();

  if (!resume)
    init_uart_instance(base_baud);

  if (!_uart->startup(reg))
    return false;

  add_state(ENABLED);
  return true;
}

IMPLEMENT L4::Uart *
Uart::uart()
{
  return _uart;
}

//------------------------------------------------------
IMPLEMENTATION [!second_uart]:

IMPLEMENT void inline
Uart::init_uart_instance(Unsigned32 base_baud)
{
  _uart = _the_uart.construct(base_baud);
}

//------------------------------------------------------
IMPLEMENTATION [second_uart]:

EXTENSION class Uart
{
  static unsigned _instance;
};

unsigned Uart::_instance;
Static_object<FIASCO_UART2_TYPE> _the_uart_2;

IMPLEMENT void inline
Uart::init_uart_instance(Unsigned32 base_baud)
{
  if (_instance)
    _uart = _the_uart_2.construct(base_baud);
  else
    _uart = _the_uart.construct(base_baud);
}

PUBLIC bool
Uart::set_uart_instance(unsigned instance)
{
  if (state() & ENABLED)
    return false; // too late -- Uart::startup() already called

  _instance = instance;
  return true;
}
