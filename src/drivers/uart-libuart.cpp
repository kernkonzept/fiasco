INTERFACE [libuart]:

#include "io_regblock.h"
#include "global_data.h"
#include "types.h"

EXTENSION class Uart
{
private:
  static void init_uart_instance(Unsigned32 base_baud);
  /** Called _before_ executing startup() of the UART driver. */
  void bsp_init();

  /** Called _after_ executing startup() of the UART driver. */
  void bsp_init_2(L4::Uart *);

  int _irq = -1;
  static Global_data<L4::Uart *> _uart;
};

//------------------------------------------------------
IMPLEMENTATION [libuart]:

DEFINE_GLOBAL Global_data<Static_object<FIASCO_UART_TYPE>> _the_uart;

DEFINE_GLOBAL Global_data<L4::Uart *> Uart::_uart;

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

IMPLEMENT_DEFAULT
void
Uart::bsp_init_2(L4::Uart *)
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

  bsp_init_2(_uart);

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
